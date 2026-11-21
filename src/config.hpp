#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Config {
    enum class Encryption { prefer, require, disable };

    std::string save_path;  // default: ~/Downloads
    std::string listen_interfaces = "0.0.0.0:6881,[::]:6881";
    std::string outgoing_interface; // e.g. "tun0" to bind all traffic to a VPN
    Encryption encryption = Encryption::prefer;
    bool dht = true;
    bool lsd = true;
    bool upnp = false;   // off: don't open router ports unless asked to
    bool natpmp = false;
    bool anonymous_mode = false;
    int download_limit_kib = 0; // 0 = unlimited
    int upload_limit_kib = 0;
    int max_active_downloads = 3;
    int max_active_seeds = 5;
};

// Load `path` into `cfg`. A missing file is created with the defaults
// (commented). Unknown keys and bad values are reported in `warnings` and
// leave the default in place.
void load_config(const std::filesystem::path& path, Config& cfg, std::vector<std::string>& warnings);
