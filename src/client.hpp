#pragma once

#include "config.hpp"

#include <libtorrent/download_priority.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <deque>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct LogEntry {
    std::time_t time;
    bool error;
    std::string text; // untrusted (may contain peer/tracker text): sanitize before display
};

struct FileEntry {
    lt::file_index_t index;
    std::string path; // untrusted
    std::int64_t size;
};

class Client {
public:
    Client(Config cfg, std::filesystem::path data_dir);
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool start(std::string& err);
    // Process alerts, refresh status, save resume data periodically.
    void tick();
    // Save resume data for every torrent and the session state.
    void shutdown();

    // Torrents sorted by queue position (seeding ones last, by name).
    std::vector<lt::torrent_status> torrents() const;
    const std::deque<LogEntry>& log() const { return log_; }

    // Add a magnet link, bare info-hash or .torrent file. The save path
    // is validated and created. Errors are returned and also logged.
    bool add(const std::string& source, const std::string& save_path, std::string& err);

    void toggle_pause(const lt::torrent_handle& h);
    void recheck(const lt::torrent_handle& h);
    void remove(const lt::torrent_handle& h, bool delete_files);
    void queue_up(const lt::torrent_handle& h);
    void queue_down(const lt::torrent_handle& h);

    std::vector<FileEntry> files(const lt::torrent_handle& h) const;
    std::vector<lt::download_priority_t> file_priorities(const lt::torrent_handle& h) const;
    std::vector<std::int64_t> file_progress(const lt::torrent_handle& h) const;
    void set_file_priorities(const lt::torrent_handle& h, const std::vector<lt::download_priority_t>& p);

    const Config& config() const { return cfg_; }
    void info(const std::string& msg) { push_log(false, msg); }
    void error(const std::string& msg) { push_log(true, msg); }

private:
    void push_log(bool error, std::string text);
    void handle_alerts();
    void load_resume_files();
    void request_resume(const lt::torrent_handle& h, bool always);
    std::filesystem::path resume_path(const lt::info_hash_t& ih) const;

    Config cfg_;
    std::filesystem::path data_dir_, resume_dir_, state_file_;
    std::unique_ptr<lt::session> ses_;
    std::unordered_map<lt::torrent_handle, lt::torrent_status> status_;
    std::set<std::string> removed_; // info-hash hex of removed torrents
    std::deque<LogEntry> log_;
    int outstanding_resume_ = 0;
    std::chrono::steady_clock::time_point last_save_;
};

// Short human status: downloading, seeding, paused, queued, error, ...
std::string status_text(const lt::torrent_status& st);
std::string info_hash_hex(const lt::info_hash_t& ih);
