#include "ui.hpp"

#include "util.hpp"

#ifndef NCURSES_WIDECHAR
#define NCURSES_WIDECHAR 1
#endif
#include <curses.h>

#include <algorithm>
#include <cwctype>

namespace {

enum Pair : short { P_OK = 1, P_ERR, P_ACTIVE, P_IDLE, P_BAR };

constexpr wint_t ESC = 27;
constexpr std::size_t kMaxInput = 8192;

struct Column {
    const wchar_t* title;
    int width;
    int priority; // lower = shown first when space is short
    bool right;
};

// Name is the flexible column; these are dropped right-to-left by priority
// when the terminal is narrow.
const Column kColumns[] = {
    {L"Done", 7, 1, true},   {L"Status", 11, 1, false}, {L"Down", 11, 2, true},
    {L"Size", 10, 3, false}, {L"Up", 11, 3, true},       {L"ETA", 8, 4, true},
    {L"Peers", 9, 5, true},  {L"Ratio", 6, 6, true},
};
constexpr int kNameMin = 12;

std::wstring w(const std::string& s) { return util::sanitize(s); }

bool is_enter(wint_t ch, bool key) { return (!key && (ch == L'\n' || ch == L'\r')) || (key && ch == KEY_ENTER); }
bool is_backspace(wint_t ch, bool key) { return (key && ch == KEY_BACKSPACE) || (!key && (ch == 127 || ch == 8)); }

int status_pair(const lt::torrent_status& st) {
    std::string s = status_text(st);
    if (s == "error") return P_ERR;
    if (s == "seeding" || s == "finished") return P_OK;
    if (s == "paused" || s == "queued") return P_IDLE;
    return P_ACTIVE;
}

// Which columns fit in `cols` terminal columns (always keeps Name).
std::vector<const Column*> visible_columns(int cols, int& name_w) {
    std::vector<const Column*> shown;
    for (int prio = 1; prio <= 6; ++prio) {
        for (const auto& c : kColumns) {
            if (c.priority != prio) continue;
            int used = kNameMin;
            for (auto* s : shown) used += s->width + 1;
            if (used + c.width + 1 <= cols) shown.push_back(&c);
        }
    }
    // keep the table order
    std::vector<const Column*> ordered;
    for (const auto& c : kColumns)
        if (std::find(shown.begin(), shown.end(), &c) != shown.end()) ordered.push_back(&c);
    name_w = cols;
    for (auto* c : ordered) name_w -= c->width + 1;
    return ordered;
}

std::string cell(const Column& c, const lt::torrent_status& st) {
    std::wstring t = c.title;
    if (t == L"Done") return util::format_percent(st.progress);
    if (t == L"Status") return status_text(st);
    if (t == L"Down") return util::format_rate(st.download_payload_rate);
    if (t == L"Up") return util::format_rate(st.upload_payload_rate);
    if (t == L"Size") return st.has_metadata ? util::format_bytes(st.total_wanted) : "-";
    if (t == L"ETA") {
        std::int64_t left = st.total_wanted - st.total_wanted_done;
        if (st.download_payload_rate <= 0 || left <= 0 || status_text(st) != "downloading") return "-";
        return util::format_eta(left / st.download_payload_rate);
    }
    if (t == L"Peers") return std::to_string(st.num_seeds) + "/" + std::to_string(st.num_peers);
    if (t == L"Ratio") {
        std::int64_t down = st.all_time_download > 0 ? st.all_time_download : st.total_wanted_done;
        return down > 0 ? util::format_ratio(static_cast<double>(st.all_time_upload) / down) : "-";
    }
    return "";
}

std::string display_name(const lt::torrent_status& st) {
    if (!st.name.empty()) return st.name;
    return "[" + info_hash_hex(st.info_hashes).substr(0, 12) + "…]"; // magnet without metadata yet
}

} // namespace

Ui::Ui(Client& client) : c_(client) {
    initscr();
    cbreak(); // ^C still raises SIGINT, which shuts down cleanly
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    timeout(250);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(P_OK, COLOR_GREEN, -1);
        init_pair(P_ERR, COLOR_RED, -1);
        init_pair(P_ACTIVE, COLOR_CYAN, -1);
        init_pair(P_IDLE, COLOR_YELLOW, -1);
        init_pair(P_BAR, COLOR_BLACK, COLOR_YELLOW);
        colors_ = true;
    }
}

Ui::~Ui() { endwin(); }

void Ui::put(int y, int x, int width, const std::wstring& s, int attr, bool right) {
    if (width <= 0 || y < 0 || y >= LINES || x >= COLS) return;
    width = std::min(width, COLS - x);
    std::wstring t = util::fit(s, width, right);
    if (attr) attron(attr);
    // explicit length, never a format string
    mvaddnwstr(y, x, t.c_str(), static_cast<int>(t.size()));
    if (attr) attroff(attr);
}

void Ui::refresh_data() {
    rows_ = c_.torrents();
    int found = -1;
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
        if (rows_[i].handle == sel_) found = i;
    if (found < 0) {
        // selection gone (removed): stay at the same position
        sel_idx_ = std::clamp(sel_idx_, 0, std::max(0, static_cast<int>(rows_.size()) - 1));
        sel_ = rows_.empty() ? lt::torrent_handle() : rows_[sel_idx_].handle;
        if (mode_ == Mode::files) mode_ = Mode::list;
    } else {
        sel_idx_ = found;
    }
    if (mode_ == Mode::files) {
        if (files_.empty()) open_files(); // metadata may have just arrived
        fprog_ = c_.file_progress(sel_);
    }
}

void Ui::draw() {
    erase();
    if (LINES < 8 || COLS < 40) {
        put(0, 0, COLS, L"Terminal too small");
        refresh();
        return;
    }
    draw_header();
    int body_y = 1, body_h = LINES - 3;
    switch (mode_) {
    case Mode::list: draw_list(body_y, body_h); break;
    case Mode::files: {
        int top_h = std::max(4, body_h / 2);
        draw_list(body_y, top_h);
        draw_files(body_y + top_h, body_h - top_h);
        break;
    }
    case Mode::log: draw_log(body_y, body_h); break;
    case Mode::help: draw_help(body_y, body_h); break;
    }
    draw_footer();
    wnoutrefresh(stdscr);
}

void Ui::draw_header() {
    std::int64_t down = 0, up = 0;
    for (const auto& st : rows_) {
        down += st.download_payload_rate;
        up += st.upload_payload_rate;
    }
    int bar = colors_ ? COLOR_PAIR(P_BAR) : A_REVERSE;
    std::string right = "down " + util::format_rate(down) + "  up " + util::format_rate(up) + "  " +
                        std::to_string(rows_.size()) + " torrent" + (rows_.size() == 1 ? "" : "s") + " ";
    put(0, 0, COLS, L" p2padga", bar | A_BOLD);
    put(0, 9, COLS - 9, w(right), bar, true);
}

void Ui::draw_list(int y, int h) {
    int name_w = 0;
    auto cols = visible_columns(COLS, name_w);
    // column titles
    put(y, 0, COLS, L"", A_BOLD | A_UNDERLINE);
    put(y, 0, name_w, L"Name", A_BOLD | A_UNDERLINE);
    int x = name_w + 1;
    for (auto* c : cols) {
        put(y, x, c->width, c->title, A_BOLD | A_UNDERLINE, c->right);
        x += c->width + 1;
    }
    int rows_h = h - 1;
    if (rows_.empty()) {
        put(y + 2, 2, COLS - 4, L"No torrents yet. Press 'a' to add a magnet link or a .torrent file.");
        return;
    }
    if (sel_idx_ < list_top_) list_top_ = sel_idx_;
    if (sel_idx_ >= list_top_ + rows_h) list_top_ = sel_idx_ - rows_h + 1;
    list_top_ = std::clamp(list_top_, 0, std::max(0, static_cast<int>(rows_.size()) - rows_h));
    for (int r = 0; r < rows_h && list_top_ + r < static_cast<int>(rows_.size()); ++r) {
        const auto& st = rows_[list_top_ + r];
        bool selected = list_top_ + r == sel_idx_;
        int base = selected ? A_REVERSE : 0;
        int ry = y + 1 + r;
        put(ry, 0, COLS, L"", base);
        put(ry, 0, name_w, w(display_name(st)), base);
        int cx = name_w + 1;
        for (auto* c : cols) {
            int attr = base;
            if (colors_ && !selected && std::wstring(c->title) == L"Status") attr |= COLOR_PAIR(status_pair(st));
            put(ry, cx, c->width, w(cell(*c, st)), attr, c->right);
            cx += c->width + 1;
        }
    }
}

void Ui::draw_files(int y, int h) {
    const auto it = std::find_if(rows_.begin(), rows_.end(), [&](auto& s) { return s.handle == sel_; });
    std::string name = it != rows_.end() ? display_name(*it) : "";
    put(y, 0, COLS, w(" Files: " + name), A_BOLD | A_REVERSE);
    if (files_.empty()) {
        put(y + 2, 2, COLS - 4, L"Waiting for metadata from peers...");
        return;
    }
    int rows_h = h - 1;
    if (fsel_ < ftop_) ftop_ = fsel_;
    if (fsel_ >= ftop_ + rows_h) ftop_ = fsel_ - rows_h + 1;
    for (int r = 0; r < rows_h && ftop_ + r < static_cast<int>(files_.size()); ++r) {
        const auto& f = files_[ftop_ + r];
        auto idx = static_cast<std::size_t>(static_cast<int>(f.index));
        bool on = idx >= prios_.size() || prios_[idx] != lt::dont_download;
        double done = (idx < fprog_.size() && f.size > 0) ? static_cast<double>(fprog_[idx]) / f.size : 0.0;
        bool selected = ftop_ + r == fsel_;
        int base = selected ? A_REVERSE : 0;
        int ry = y + 1 + r;
        put(ry, 0, COLS, L"", base);
        put(ry, 1, 3, on ? L"[x]" : L"[ ]", base);
        put(ry, 5, 7, w(util::format_percent(done)), base, true);
        put(ry, 13, 10, w(util::format_bytes(f.size)), base, true);
        put(ry, 24, COLS - 24, w(f.path), base | (on ? 0 : A_DIM));
    }
}

void Ui::draw_log(int y, int h) {
    put(y, 0, COLS, L" Log", A_BOLD | A_REVERSE);
    const auto& log = c_.log();
    int n = static_cast<int>(log.size());
    int rows_h = h - 1;
    log_scroll_ = std::clamp(log_scroll_, 0, std::max(0, n - rows_h));
    int first = std::max(0, n - rows_h - log_scroll_);
    for (int r = 0; r < rows_h && first + r < n; ++r) {
        const auto& e = log[static_cast<std::size_t>(first + r)];
        char ts[16];
        std::strftime(ts, sizeof ts, "%H:%M:%S", std::localtime(&e.time));
        int attr = (e.error && colors_) ? COLOR_PAIR(P_ERR) : 0;
        put(y + 1 + r, 1, COLS - 2, w(std::string(ts) + "  " + e.text), attr);
    }
}

void Ui::draw_help(int y, int) {
    static const wchar_t* lines[] = {
        L" Keys",
        L"",
        L"  a          add a magnet link, info-hash or .torrent file",
        L"  j k / ↑ ↓  move        g G  first / last",
        L"  enter / l  files of the selected torrent",
        L"  p / space  pause or resume",
        L"  + -        move up / down in the download queue",
        L"  r          recheck downloaded data",
        L"  d          remove torrent (keeps the downloaded files)",
        L"  D          remove torrent AND delete its files",
        L"  L          log      ?  this help      q  quit",
        L"",
        L"  In the file list: space toggles a file, a = all, n = none, q = back",
        L"",
        L"  Settings: ~/.config/p2padga/config",
    };
    int i = 0;
    for (const wchar_t* l : lines) {
        put(y + i, 0, COLS, l, i == 0 ? A_BOLD | A_REVERSE : 0);
        ++i;
    }
}

void Ui::draw_footer() {
    // latest log line, if recent
    const auto& log = c_.log();
    if (!log.empty() && std::time(nullptr) - log.back().time < 10) {
        int attr = (log.back().error && colors_) ? COLOR_PAIR(P_ERR) : 0;
        put(LINES - 2, 0, COLS, w(" " + log.back().text), attr);
    }
    const wchar_t* hints = L"";
    switch (mode_) {
    case Mode::list: hints = L" a add  p pause  enter files  d remove  L log  ? help  q quit"; break;
    case Mode::files: hints = L" space toggle  a all  n none  q back"; break;
    case Mode::log: hints = L" j k scroll  q back"; break;
    case Mode::help: hints = L" any key: back"; break;
    }
    put(LINES - 1, 0, COLS, hints, colors_ ? COLOR_PAIR(P_BAR) : A_REVERSE);
}

void Ui::run(const std::atomic<bool>& stop) {
    stop_ = &stop;
    while (!stop) {
        c_.tick();
        refresh_data();
        draw();
        doupdate();
        wint_t ch;
        int r = get_wch(&ch);
        if (r == ERR) continue;
        if (!handle(ch, r == KEY_CODE_YES)) break;
    }
}

bool Ui::handle(wint_t ch, bool key) {
    if (key && ch == KEY_RESIZE) return true;
    switch (mode_) {
    case Mode::help:
        mode_ = Mode::list;
        return true;
    case Mode::log:
        handle_log(ch, key);
        return true;
    case Mode::files:
        handle_files(ch, key);
        return true;
    case Mode::list:
        if (!key && ch == L'q') return false;
        handle_list(ch, key);
        return true;
    }
    return true;
}

void Ui::handle_list(wint_t ch, bool key) {
    int n = static_cast<int>(rows_.size());
    auto select = [&](int i) {
        if (n == 0) return;
        sel_idx_ = std::clamp(i, 0, n - 1);
        sel_ = rows_[sel_idx_].handle;
    };
    int page = std::max(1, LINES - 5);
    if ((key && ch == KEY_DOWN) || ch == L'j') select(sel_idx_ + 1);
    else if ((key && ch == KEY_UP) || ch == L'k') select(sel_idx_ - 1);
    else if ((key && ch == KEY_HOME) || ch == L'g') select(0);
    else if ((key && ch == KEY_END) || ch == L'G') select(n - 1);
    else if (key && ch == KEY_NPAGE) select(sel_idx_ + page);
    else if (key && ch == KEY_PPAGE) select(sel_idx_ - page);
    else if (!key && ch == L'a') add_torrent();
    else if (!key && ch == L'L') { mode_ = Mode::log; log_scroll_ = 0; }
    else if (!key && ch == L'?') mode_ = Mode::help;
    else if (n == 0) return;
    else if (is_enter(ch, key) || (key && ch == KEY_RIGHT) || ch == L'l' || ch == L'i') { open_files(); mode_ = Mode::files; }
    else if (ch == L'p' || ch == L' ') c_.toggle_pause(sel_);
    else if (ch == L'+' || ch == L'=') c_.queue_up(sel_);
    else if (ch == L'-') c_.queue_down(sel_);
    else if (ch == L'r') {
        if (confirm("Recheck all downloaded data of this torrent?")) c_.recheck(sel_);
    } else if (ch == L'd') {
        if (confirm("Remove this torrent? Downloaded files are kept.")) c_.remove(sel_, false);
    } else if (ch == L'D') {
        if (confirm("Remove this torrent AND DELETE its downloaded files?")) c_.remove(sel_, true);
    }
}

void Ui::handle_files(wint_t ch, bool key) {
    int n = static_cast<int>(files_.size());
    if (ch == L'q' || ch == L'h' || ch == ESC || (key && (ch == KEY_LEFT || ch == KEY_BACKSPACE))) {
        mode_ = Mode::list;
        files_.clear();
        return;
    }
    if (n == 0) return;
    int page = std::max(1, LINES / 2 - 3);
    if ((key && ch == KEY_DOWN) || ch == L'j') fsel_ = std::min(n - 1, fsel_ + 1);
    else if ((key && ch == KEY_UP) || ch == L'k') fsel_ = std::max(0, fsel_ - 1);
    else if (key && ch == KEY_NPAGE) fsel_ = std::min(n - 1, fsel_ + page);
    else if (key && ch == KEY_PPAGE) fsel_ = std::max(0, fsel_ - page);
    else if ((key && ch == KEY_HOME) || ch == L'g') fsel_ = 0;
    else if ((key && ch == KEY_END) || ch == L'G') fsel_ = n - 1;
    else if (ch == L' ' || is_enter(ch, key)) toggle_file(false, false);
    else if (ch == L'a') toggle_file(true, true);
    else if (ch == L'n') toggle_file(true, false);
}

void Ui::handle_log(wint_t ch, bool key) {
    if (ch == L'q' || ch == ESC || (key && ch == KEY_LEFT)) mode_ = Mode::list;
    else if ((key && ch == KEY_UP) || ch == L'k') ++log_scroll_;
    else if ((key && ch == KEY_DOWN) || ch == L'j') log_scroll_ = std::max(0, log_scroll_ - 1);
    else if (key && ch == KEY_PPAGE) log_scroll_ += LINES - 4;
    else if (key && ch == KEY_NPAGE) log_scroll_ = std::max(0, log_scroll_ - (LINES - 4));
}

void Ui::open_files() {
    files_ = c_.files(sel_);
    prios_ = c_.file_priorities(sel_);
    fprog_ = c_.file_progress(sel_);
    fsel_ = std::clamp(fsel_, 0, std::max(0, static_cast<int>(files_.size()) - 1));
}

void Ui::toggle_file(bool all, bool on) {
    if (files_.empty()) return;
    std::size_t needed = 0;
    for (const auto& f : files_) needed = std::max(needed, static_cast<std::size_t>(static_cast<int>(f.index)) + 1);
    if (prios_.size() < needed) prios_.resize(needed, lt::default_priority);
    if (all) {
        for (const auto& f : files_)
            prios_[static_cast<std::size_t>(static_cast<int>(f.index))] = on ? lt::default_priority : lt::dont_download;
    } else {
        auto& p = prios_[static_cast<std::size_t>(static_cast<int>(files_[fsel_].index))];
        p = (p == lt::dont_download) ? lt::default_priority : lt::dont_download;
    }
    c_.set_file_priorities(sel_, prios_);
}

void Ui::add_torrent() {
    auto src = prompt("Add: magnet link, info-hash or path to a .torrent file", "");
    if (!src || util::trim(*src).empty()) return;
    auto dir = prompt("Save to", c_.config().save_path);
    if (!dir) return;
    std::string err;
    if (c_.add(*src, *dir, err)) c_.info("Adding torrent...");
    else c_.error("Could not add: " + err);
}

std::optional<std::string> Ui::prompt(const std::string& title, const std::string& initial) {
    std::wstring buf = w(initial);
    curs_set(1);
    std::optional<std::string> result;
    while (!*stop_) {
        c_.tick();
        refresh_data();
        draw();
        int bw = std::min(COLS - 2, 78), bh = 5;
        WINDOW* win = newwin(bh, bw, (LINES - bh) / 2, (COLS - bw) / 2);
        if (!win) break;
        keypad(win, TRUE);
        wtimeout(win, 250);
        box(win, 0, 0);
        std::wstring t = util::fit(L" " + w(title) + L" ", bw - 4);
        mvwaddnwstr(win, 0, 2, t.c_str(), static_cast<int>(t.size()));
        std::wstring hint = util::fit(L" enter: OK  esc: cancel  ^U: clear ", bw - 4, true);
        mvwaddnwstr(win, bh - 1, 2, hint.c_str(), static_cast<int>(hint.size()));
        // show the end of the input when it's longer than the box
        int field = bw - 4;
        std::wstring shown = buf;
        while (util::display_width(shown) > field - 1) shown.erase(0, 1);
        mvwaddnwstr(win, 2, 2, shown.c_str(), static_cast<int>(shown.size()));
        wmove(win, 2, 2 + util::display_width(shown));
        wnoutrefresh(win);
        doupdate();

        wint_t ch;
        int r = wget_wch(win, &ch);
        delwin(win);
        if (r == ERR) continue;
        bool key = r == KEY_CODE_YES;
        if (key && ch == KEY_RESIZE) continue;
        if (is_enter(ch, key)) { result = util::to_utf8(buf); break; }
        if (!key && ch == ESC) break;
        if (is_backspace(ch, key)) { if (!buf.empty()) buf.pop_back(); }
        else if (!key && ch == 21) buf.clear(); // ^U
        else if (!key && ch == 23) {             // ^W: delete a word
            while (!buf.empty() && buf.back() == L' ') buf.pop_back();
            while (!buf.empty() && buf.back() != L' ') buf.pop_back();
        } else if (!key && std::iswprint(ch) && buf.size() < kMaxInput) {
            buf += static_cast<wchar_t>(ch);
        }
    }
    curs_set(0);
    return result;
}

bool Ui::confirm(const std::string& question) {
    while (!*stop_) {
        c_.tick();
        refresh_data();
        draw();
        int bw = std::min(COLS - 2, std::max(40, util::display_width(w(question)) + 6)), bh = 5;
        WINDOW* win = newwin(bh, bw, (LINES - bh) / 2, (COLS - bw) / 2);
        if (!win) return false;
        keypad(win, TRUE);
        wtimeout(win, 250);
        box(win, 0, 0);
        std::wstring q = util::fit(w(question), bw - 4);
        mvwaddnwstr(win, 1, 2, q.c_str(), static_cast<int>(q.size()));
        std::wstring yn = util::fit(L"y: yes   any other key: no", bw - 4);
        mvwaddnwstr(win, 3, 2, yn.c_str(), static_cast<int>(yn.size()));
        wnoutrefresh(win);
        doupdate();
        wint_t ch;
        int r = wget_wch(win, &ch);
        delwin(win);
        if (r == ERR) continue;
        if (r == KEY_CODE_YES && ch == KEY_RESIZE) continue;
        return r != KEY_CODE_YES && (ch == L'y' || ch == L'Y');
    }
    return false;
}
