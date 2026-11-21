#include "config.hpp"

#include "util.hpp"

#include <charconv>
#include <sstream>

namespace {

const char* kDefaultConfig = R"(# p2padga configuration
# Lines are "key = value"; "#" starts a comment.

# Where new torrents are saved (you can change it per torrent when adding).
# save_path = ~/Downloads

# Addresses to listen on for incoming peers.
# listen_interfaces = 0.0.0.0:6881,[::]:6881

# Bind all outgoing traffic to one interface, e.g. your VPN's (tun0, wg0).
# If that interface goes down, connections fail instead of leaking out
# through your normal connection.
# outgoing_interface =

# Peer connection encryption: prefer, require or disable.
# Note: this hides BitTorrent traffic from simple throttling; it does NOT
# make you anonymous. Peers and trackers still see your IP address.
# encryption = prefer

# Peer discovery.
# dht = true
# lsd = true            # local service discovery (your LAN)

# Ask your router to open a port. Off by default: it exposes a port on
# your network without asking.
# upnp = false
# natpmp = false

# Don't send identifying info (client name, local IPs) to peers/trackers.
# anonymous_mode = false

# Speed limits in KiB/s, 0 = unlimited.
# download_limit = 0
# upload_limit = 0

# Queue sizes.
# max_active_downloads = 3
# max_active_seeds = 5
)";

bool parse_bool(const std::string& v, bool& out) {
    if (v == "true" || v == "yes" || v == "on" || v == "1") { out = true; return true; }
    if (v == "false" || v == "no" || v == "off" || v == "0") { out = false; return true; }
    return false;
}

bool parse_int(const std::string& v, int lo, int hi, int& out) {
    int n = 0;
    auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
    if (ec != std::errc() || ptr != v.data() + v.size() || n < lo || n > hi) return false;
    out = n;
    return true;
}

// interface names / address lists: printable ASCII only, no spaces
bool plain(const std::string& v) {
    for (unsigned char c : v)
        if (c <= 0x20 || c >= 0x7f) return false;
    return true;
}

} // namespace

void load_config(const std::filesystem::path& path, Config& cfg, std::vector<std::string>& warnings) {
    std::string err;
    auto data = util::read_file(path, 1 << 20, err, true);
    if (!data) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::vector<char> buf(kDefaultConfig, kDefaultConfig + std::char_traits<char>::length(kDefaultConfig));
            if (!util::write_file_atomic(path, buf, err)) warnings.push_back(err);
        } else {
            warnings.push_back(err);
        }
        return;
    }

    std::istringstream in(std::string(data->begin(), data->end()));
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        line = util::trim(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        auto where = path.filename().string() + ":" + std::to_string(lineno);
        if (eq == std::string::npos) {
            warnings.push_back(where + ": expected key = value");
            continue;
        }
        std::string key = util::trim(line.substr(0, eq));
        std::string val = util::trim(line.substr(eq + 1));
        bool ok = true;

        if (key == "save_path") cfg.save_path = val;
        else if (key == "listen_interfaces") ok = plain(val) && !val.empty() && (cfg.listen_interfaces = val, true);
        else if (key == "outgoing_interface") ok = plain(val) && (cfg.outgoing_interface = val, true);
        else if (key == "encryption") {
            if (val == "prefer") cfg.encryption = Config::Encryption::prefer;
            else if (val == "require") cfg.encryption = Config::Encryption::require;
            else if (val == "disable") cfg.encryption = Config::Encryption::disable;
            else ok = false;
        }
        else if (key == "dht") ok = parse_bool(val, cfg.dht);
        else if (key == "lsd") ok = parse_bool(val, cfg.lsd);
        else if (key == "upnp") ok = parse_bool(val, cfg.upnp);
        else if (key == "natpmp") ok = parse_bool(val, cfg.natpmp);
        else if (key == "anonymous_mode") ok = parse_bool(val, cfg.anonymous_mode);
        else if (key == "download_limit") ok = parse_int(val, 0, 4'000'000, cfg.download_limit_kib);
        else if (key == "upload_limit") ok = parse_int(val, 0, 4'000'000, cfg.upload_limit_kib);
        else if (key == "max_active_downloads") ok = parse_int(val, -1, 1000, cfg.max_active_downloads);
        else if (key == "max_active_seeds") ok = parse_int(val, -1, 1000, cfg.max_active_seeds);
        else {
            warnings.push_back(where + ": unknown setting \"" + key + "\"");
            continue;
        }
        if (!ok) warnings.push_back(where + ": invalid value for " + key + ", using the default");
    }
}
