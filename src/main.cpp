#include "client.hpp"
#include "config.hpp"
#include "ui.hpp"
#include "util.hpp"

#include <libtorrent/version.hpp>

#include <atomic>
#include <clocale>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <langinfo.h>
#include <sys/file.h>
#include <unistd.h>

#ifndef P2PADGA_VERSION
#define P2PADGA_VERSION "1.0.0"
#endif

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void usage(std::ostream& out) {
    out << "usage: p2padga [options] [MAGNET | INFO-HASH | FILE.torrent ...]\n"
           "\n"
           "A terminal BitTorrent client. Torrents given on the command line are\n"
           "added to the save path.\n"
           "\n"
           "options:\n"
           "  -s, --save-path DIR   save new torrents in DIR (default from the config)\n"
           "  -c, --config FILE     use FILE instead of ~/.config/p2padga/config\n"
           "  -h, --help            show this help\n"
           "  -v, --version         show the version\n";
}

} // namespace

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "");
    util::set_ascii_only(std::strcmp(nl_langinfo(CODESET), "UTF-8") != 0);

    std::string save_path_arg, config_arg;
    std::vector<std::string> sources;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need_value = [&](const char* opt) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "p2padga: " << opt << " needs a value\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") { usage(std::cout); return 0; }
        if (a == "-v" || a == "--version") {
            std::cout << "p2padga " P2PADGA_VERSION " (libtorrent " << lt::version() << ")\n";
            return 0;
        }
        if (a == "-s" || a == "--save-path") save_path_arg = need_value("--save-path");
        else if (a == "-c" || a == "--config") config_arg = need_value("--config");
        else if (a == "--") { for (++i; i < argc; ++i) sources.emplace_back(argv[i]); }
        else if (a.size() > 1 && a[0] == '-') { std::cerr << "p2padga: unknown option " << a << "\n\n"; usage(std::cerr); return 2; }
        else sources.push_back(a);
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        std::cerr << "p2padga: needs an interactive terminal\n";
        return 1;
    }

    std::string err;
    std::vector<std::string> warnings;
    Config cfg;
    std::filesystem::path cfg_path = config_arg.empty() ? util::config_dir() / "config" : std::filesystem::path(config_arg);
    if (config_arg.empty() && !util::ensure_private_dir(util::config_dir(), err)) warnings.push_back(err);
    load_config(cfg_path, cfg, warnings);
    if (!save_path_arg.empty()) cfg.save_path = save_path_arg;
    if (cfg.save_path.empty()) cfg.save_path = "~/Downloads";

    // one instance at a time: two would overwrite each other's resume data
    auto data = util::data_dir();
    if (!util::ensure_private_dir(data, err)) {
        std::cerr << "p2padga: " << err << "\n";
        return 1;
    }
    int lock_fd = ::open((data / "lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock_fd < 0 || ::flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        std::cerr << "p2padga: another p2padga is already running (" << (data / "lock").string() << ")\n";
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN);

    try {
        Client client(cfg, data);
        if (!client.start(err)) {
            std::cerr << "p2padga: " << err << "\n";
            return 1;
        }
        for (const auto& wmsg : warnings) client.error(wmsg);
        for (const auto& src : sources) {
            if (!client.add(src, cfg.save_path, err)) client.error("Could not add " + src + ": " + err);
        }
        {
            Ui ui(client);
            ui.run(g_stop);
        } // terminal restored here
        std::cerr << "p2padga: saving state...\n";
        client.shutdown();
    } catch (const std::exception& e) {
        std::cerr << "p2padga: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
