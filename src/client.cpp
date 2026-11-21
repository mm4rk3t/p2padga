#include "client.hpp"

#include "util.hpp"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <tuple>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxTorrentFile = 32 << 20;  // .torrent files
constexpr std::size_t kMaxResumeFile = 64 << 20;   // resume data incl. info dict
constexpr std::size_t kMaxStateFile = 8 << 20;     // session (DHT) state
constexpr std::size_t kMaxLog = 500;

bool istarts_with(const std::string& s, const char* prefix) {
    for (std::size_t i = 0; prefix[i]; ++i)
        if (i >= s.size() || std::tolower(static_cast<unsigned char>(s[i])) != prefix[i]) return false;
    return true;
}

bool is_bare_info_hash(const std::string& s) {
    auto all = [&](auto pred) { return std::all_of(s.begin(), s.end(), [&](unsigned char c) { return pred(c); }); };
    if (s.size() == 40) return all([](unsigned char c) { return std::isxdigit(c) != 0; });
    if (s.size() == 32) return all([](unsigned char c) { return std::isalnum(c) != 0; }); // base32
    return false;
}

lt::settings_pack make_settings(const Config& cfg) {
    lt::settings_pack sp;
    sp.set_str(lt::settings_pack::listen_interfaces, cfg.listen_interfaces);
    if (!cfg.outgoing_interface.empty())
        sp.set_str(lt::settings_pack::outgoing_interfaces, cfg.outgoing_interface);

    sp.set_bool(lt::settings_pack::enable_dht, cfg.dht);
    sp.set_bool(lt::settings_pack::enable_lsd, cfg.lsd);
    sp.set_bool(lt::settings_pack::enable_upnp, cfg.upnp);
    sp.set_bool(lt::settings_pack::enable_natpmp, cfg.natpmp);
    sp.set_bool(lt::settings_pack::anonymous_mode, cfg.anonymous_mode);

    using S = lt::settings_pack;
    switch (cfg.encryption) {
    case Config::Encryption::require:
        sp.set_int(S::out_enc_policy, S::pe_forced);
        sp.set_int(S::in_enc_policy, S::pe_forced);
        sp.set_int(S::allowed_enc_level, S::pe_rc4); // full stream encryption only
        sp.set_bool(S::prefer_rc4, true);
        break;
    case Config::Encryption::prefer:
        sp.set_int(S::out_enc_policy, S::pe_enabled);
        sp.set_int(S::in_enc_policy, S::pe_enabled);
        sp.set_int(S::allowed_enc_level, S::pe_both);
        sp.set_bool(S::prefer_rc4, true);
        break;
    case Config::Encryption::disable:
        sp.set_int(S::out_enc_policy, S::pe_disabled);
        sp.set_int(S::in_enc_policy, S::pe_disabled);
        break;
    }

    // Hardening (several are libtorrent defaults; set explicitly so a
    // future default change can't silently weaken them):
    // - don't let peers/trackers point us at privileged ports or local
    //   services (SSRF / using us as a DDoS reflector)
    sp.set_bool(S::no_connect_privileged_ports, true);
    sp.set_bool(S::ssrf_mitigation, true);
    // - verify TLS certificates of https trackers
    sp.set_bool(S::validate_https_trackers, true);
    // - no internationalized tracker hostnames (homograph spoofing)
    sp.set_bool(S::allow_idna, false);

    sp.set_int(S::download_rate_limit, cfg.download_limit_kib * 1024);
    sp.set_int(S::upload_rate_limit, cfg.upload_limit_kib * 1024);
    sp.set_int(S::active_downloads, cfg.max_active_downloads);
    sp.set_int(S::active_seeds, cfg.max_active_seeds);

    sp.set_int(S::alert_mask, lt::alert_category::error | lt::alert_category::status | lt::alert_category::storage);
    sp.set_int(S::alert_queue_size, 10000);
    sp.set_int(S::stop_tracker_timeout, 2); // don't hang on exit
    return sp;
}

// Expand ~, make absolute, create it, and check we can write there.
std::optional<std::string> prepare_save_path(const std::string& in, std::string& err) {
    std::string expanded = util::expand_tilde(util::trim(in));
    if (expanded.empty()) {
        err = "no save path given";
        return std::nullopt;
    }
    std::error_code ec;
    fs::path p = fs::absolute(expanded, ec).lexically_normal();
    if (ec) {
        err = "bad save path: " + ec.message();
        return std::nullopt;
    }
    fs::create_directories(p, ec);
    if (ec || !fs::is_directory(p, ec)) {
        err = "cannot use " + p.string() + " as save path" + (ec ? ": " + ec.message() : "");
        return std::nullopt;
    }
    if (::access(p.c_str(), W_OK | X_OK) != 0) {
        err = "no write permission in " + p.string();
        return std::nullopt;
    }
    return p.string();
}

} // namespace

std::string info_hash_hex(const lt::info_hash_t& ih) {
    lt::sha1_hash h = ih.get_best();
    return util::to_hex(reinterpret_cast<const char*>(h.data()), h.size());
}

std::string status_text(const lt::torrent_status& st) {
    if (st.errc) return "error";
    if (st.flags & lt::torrent_flags::paused)
        return (st.flags & lt::torrent_flags::auto_managed) ? "queued" : "paused";
    switch (st.state) {
    case lt::torrent_status::checking_files: return "checking";
    case lt::torrent_status::downloading_metadata: return "metadata";
    case lt::torrent_status::downloading: return "downloading";
    case lt::torrent_status::finished: return "finished";
    case lt::torrent_status::seeding: return "seeding";
    case lt::torrent_status::checking_resume_data: return "resuming";
    default: return "?";
    }
}

Client::Client(Config cfg, fs::path data_dir)
    : cfg_(std::move(cfg)),
      data_dir_(std::move(data_dir)),
      resume_dir_(data_dir_ / "resume"),
      state_file_(data_dir_ / "session.state") {}

Client::~Client() = default;

void Client::push_log(bool error, std::string text) {
    log_.push_back({std::time(nullptr), error, std::move(text)});
    while (log_.size() > kMaxLog) log_.pop_front();
}

fs::path Client::resume_path(const lt::info_hash_t& ih) const {
    // named by info-hash only: nothing from the torrent itself ends up in
    // a file name
    return resume_dir_ / (info_hash_hex(ih) + ".resume");
}

bool Client::start(std::string& err) {
    if (!util::ensure_private_dir(data_dir_, err) || !util::ensure_private_dir(resume_dir_, err)) return false;

    lt::session_params params;
    std::string serr;
    if (auto buf = util::read_file(state_file_, kMaxStateFile, serr, true)) {
        try {
            // only the DHT state: settings always come from the config
            params = lt::read_session_params(lt::span<const char>(buf->data(), static_cast<std::ptrdiff_t>(buf->size())),
                                             lt::session::save_dht_state);
        } catch (const std::exception& e) {
            error(std::string("Ignoring damaged session state: ") + e.what());
            params = lt::session_params();
        }
    }
    params.settings = make_settings(cfg_);

    try {
        ses_ = std::make_unique<lt::session>(std::move(params));
    } catch (const std::exception& e) {
        err = std::string("cannot start the BitTorrent session: ") + e.what();
        return false;
    }
    last_save_ = std::chrono::steady_clock::now();
    load_resume_files();
    return true;
}

void Client::load_resume_files() {
    std::error_code ec;
    int n = 0;
    for (const auto& entry : fs::directory_iterator(resume_dir_, ec)) {
        if (entry.path().extension() != ".resume" || entry.is_symlink(ec) || !entry.is_regular_file(ec)) continue;
        std::string err;
        auto buf = util::read_file(entry.path(), kMaxResumeFile, err, true);
        if (!buf) {
            error("Skipping resume file: " + err);
            continue;
        }
        lt::error_code lec;
        lt::add_torrent_params atp =
            lt::read_resume_data(lt::span<const char>(buf->data(), static_cast<std::ptrdiff_t>(buf->size())), lec);
        if (lec) {
            error("Skipping damaged resume file " + entry.path().filename().string() + ": " + lec.message());
            continue;
        }
        ses_->async_add_torrent(std::move(atp));
        ++n;
    }
    if (n > 0) info("Restoring " + std::to_string(n) + " torrent(s) from the last session");
}

bool Client::add(const std::string& source, const std::string& save_path_in, std::string& err) {
    std::string src = util::trim(source);
    if (src.empty()) {
        err = "nothing to add";
        return false;
    }
    auto save_path = prepare_save_path(save_path_in, err);
    if (!save_path) return false;

    lt::add_torrent_params atp;
    if (is_bare_info_hash(src)) src = "magnet:?xt=urn:btih:" + src;
    if (istarts_with(src, "magnet:")) {
        lt::error_code ec;
        atp = lt::parse_magnet_uri(src, ec);
        if (ec) {
            err = "invalid magnet link: " + ec.message();
            return false;
        }
    } else {
        fs::path file = util::expand_tilde(src);
        auto buf = util::read_file(file, kMaxTorrentFile, err);
        if (!buf) return false;
        try {
            lt::load_torrent_limits limits;
            limits.max_buffer_size = static_cast<int>(kMaxTorrentFile);
            atp = lt::load_torrent_buffer(lt::span<const char>(buf->data(), static_cast<std::ptrdiff_t>(buf->size())),
                                          limits);
        } catch (const std::exception& e) {
            err = "invalid .torrent file: " + std::string(e.what());
            return false;
        }
    }

    if (ses_->find_torrent(atp.info_hashes.get_best()).is_valid()) {
        err = "that torrent is already in the list";
        return false;
    }
    atp.save_path = *save_path;
    removed_.erase(info_hash_hex(atp.info_hashes));
    ses_->async_add_torrent(std::move(atp));
    return true;
}

void Client::request_resume(const lt::torrent_handle& h, bool always) {
    if (!h.is_valid()) return;
    if (!always && !h.need_save_resume_data()) return;
    h.save_resume_data(lt::torrent_handle::save_info_dict);
    ++outstanding_resume_;
}

void Client::tick() {
    if (!ses_) return;
    handle_alerts();
    ses_->post_torrent_updates();
    auto now = std::chrono::steady_clock::now();
    if (now - last_save_ > std::chrono::minutes(1)) {
        last_save_ = now;
        for (const auto& [h, st] : status_) request_resume(h, false);
    }
}

void Client::handle_alerts() {
    std::vector<lt::alert*> alerts;
    ses_->pop_alerts(&alerts);
    for (lt::alert* a : alerts) {
        if (auto* at = lt::alert_cast<lt::add_torrent_alert>(a)) {
            if (at->error) {
                error("Could not add " + std::string(at->torrent_name()) + ": " + at->error.message());
                continue;
            }
            status_[at->handle] = at->handle.status();
            request_resume(at->handle, true); // persist new torrents right away
        } else if (auto* su = lt::alert_cast<lt::state_update_alert>(a)) {
            for (auto& st : su->status)
                if (status_.count(st.handle)) status_[st.handle] = std::move(st);
        } else if (auto* sr = lt::alert_cast<lt::save_resume_data_alert>(a)) {
            --outstanding_resume_;
            if (removed_.count(info_hash_hex(sr->params.info_hashes))) continue; // removed meanwhile
            std::vector<char> buf = lt::write_resume_data_buf(sr->params);
            std::string err;
            if (!util::write_file_atomic(resume_path(sr->params.info_hashes), buf, err)) error(err);
        } else if (auto* sf = lt::alert_cast<lt::save_resume_data_failed_alert>(a)) {
            --outstanding_resume_;
            if (sf->error != lt::errors::resume_data_not_modified) error(sf->message());
        } else if (auto* mr = lt::alert_cast<lt::metadata_received_alert>(a)) {
            info("Got metadata: " + std::string(mr->torrent_name()));
            request_resume(mr->handle, true);
        } else if (auto* tf = lt::alert_cast<lt::torrent_finished_alert>(a)) {
            info("Finished: " + std::string(tf->torrent_name()));
            request_resume(tf->handle, true);
        } else if (a->category() & lt::alert_category::error) {
            error(a->message());
        }
    }
}

std::vector<lt::torrent_status> Client::torrents() const {
    std::vector<lt::torrent_status> out;
    out.reserve(status_.size());
    for (const auto& [h, st] : status_) out.push_back(st);
    std::sort(out.begin(), out.end(), [](const lt::torrent_status& a, const lt::torrent_status& b) {
        auto key = [](const lt::torrent_status& s) {
            int q = static_cast<int>(s.queue_position);
            return std::make_tuple(q < 0, q, s.name);
        };
        return key(a) < key(b);
    });
    return out;
}

void Client::toggle_pause(const lt::torrent_handle& h) {
    auto it = status_.find(h);
    if (it == status_.end()) return;
    const auto& st = it->second;
    bool user_paused = (st.flags & lt::torrent_flags::paused) && !(st.flags & lt::torrent_flags::auto_managed);
    if (user_paused) {
        if (st.errc) h.clear_error();
        h.set_flags(lt::torrent_flags::auto_managed);
        h.resume();
    } else {
        h.unset_flags(lt::torrent_flags::auto_managed);
        h.pause(lt::torrent_handle::graceful_pause);
    }
    request_resume(h, true);
}

void Client::recheck(const lt::torrent_handle& h) {
    if (h.is_valid()) h.force_recheck();
}

void Client::remove(const lt::torrent_handle& h, bool delete_files) {
    auto it = status_.find(h);
    if (it == status_.end()) return;
    std::string hex = info_hash_hex(it->second.info_hashes);
    std::string name = it->second.name;
    removed_.insert(hex);
    std::error_code ec;
    fs::remove(resume_dir_ / (hex + ".resume"), ec);
    ses_->remove_torrent(h, delete_files ? lt::session::delete_files : lt::remove_flags_t{});
    status_.erase(it);
    info((delete_files ? "Removed (with its files): " : "Removed: ") + name);
}

void Client::queue_up(const lt::torrent_handle& h) {
    if (h.is_valid()) h.queue_position_up();
}

void Client::queue_down(const lt::torrent_handle& h) {
    if (h.is_valid()) h.queue_position_down();
}

std::vector<FileEntry> Client::files(const lt::torrent_handle& h) const {
    std::vector<FileEntry> out;
    if (!h.is_valid()) return out;
    auto ti = h.torrent_file();
    if (!ti) return out; // no metadata yet
    const lt::file_storage& fst = ti->files();
    for (lt::file_index_t i : fst.file_range()) {
        if (fst.pad_file_at(i)) continue;
        out.push_back({i, fst.file_path(i), fst.file_size(i)});
    }
    return out;
}

std::vector<lt::download_priority_t> Client::file_priorities(const lt::torrent_handle& h) const {
    return h.is_valid() ? h.get_file_priorities() : std::vector<lt::download_priority_t>{};
}

std::vector<std::int64_t> Client::file_progress(const lt::torrent_handle& h) const {
    return h.is_valid() ? h.file_progress(lt::torrent_handle::piece_granularity) : std::vector<std::int64_t>{};
}

void Client::set_file_priorities(const lt::torrent_handle& h, const std::vector<lt::download_priority_t>& p) {
    if (!h.is_valid()) return;
    h.prioritize_files(p);
    request_resume(h, true);
}

void Client::shutdown() {
    if (!ses_) return;
    ses_->pause();
    for (const auto& [h, st] : status_) request_resume(h, false);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (outstanding_resume_ > 0 && std::chrono::steady_clock::now() < deadline) {
        ses_->wait_for_alert(std::chrono::milliseconds(500));
        handle_alerts();
    }
    std::string err;
    std::vector<char> state = lt::write_session_params_buf(ses_->session_state(lt::session::save_dht_state));
    if (!util::write_file_atomic(state_file_, state, err)) error(err);
    ses_.reset();
}
