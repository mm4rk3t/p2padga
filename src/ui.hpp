#pragma once

#include "client.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

class Ui {
public:
    explicit Ui(Client& client);
    ~Ui();
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    // Runs until the user quits or `stop` becomes true (SIGINT/SIGTERM).
    void run(const std::atomic<bool>& stop);

private:
    enum class Mode { list, files, log, help };

    void refresh_data();
    void draw();
    void draw_header();
    void draw_list(int y, int h);
    void draw_files(int y, int h);
    void draw_log(int y, int h);
    void draw_help(int y, int h);
    void draw_footer();
    void put(int y, int x, int w, const std::wstring& s, int attr = 0, bool right = false);

    bool handle(wint_t ch, bool is_key); // false = quit
    void handle_list(wint_t ch, bool is_key);
    void handle_files(wint_t ch, bool is_key);
    void handle_log(wint_t ch, bool is_key);

    void add_torrent();
    void open_files();
    void toggle_file(bool all, bool on);
    std::optional<std::string> prompt(const std::string& title, const std::string& initial);
    bool confirm(const std::string& question);

    Client& c_;
    const std::atomic<bool>* stop_ = nullptr;
    Mode mode_ = Mode::list;
    bool colors_ = false;

    std::vector<lt::torrent_status> rows_;
    lt::torrent_handle sel_;
    int sel_idx_ = 0;
    int list_top_ = 0;

    std::vector<FileEntry> files_;
    std::vector<lt::download_priority_t> prios_;
    std::vector<std::int64_t> fprog_;
    int fsel_ = 0;
    int ftop_ = 0;

    int log_scroll_ = 0; // lines up from the newest entry
};
