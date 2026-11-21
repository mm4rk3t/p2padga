#include "util.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace util {

namespace {

bool g_ascii_only = false;

// fewer decimals as the number grows: 1.23 MiB, 12.3 MiB, 123 MiB
std::string scaled(double v, const char* unit) {
    char buf[64];
    if (v < 10) std::snprintf(buf, sizeof buf, "%.2f %s", v, unit);
    else if (v < 100) std::snprintf(buf, sizeof buf, "%.1f %s", v, unit);
    else std::snprintf(buf, sizeof buf, "%.0f %s", v, unit);
    return buf;
}

fs::path home_dir() {
    if (const char* h = std::getenv("HOME"); h && *h) return h;
    if (const passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) return pw->pw_dir;
    return "/";
}

fs::path xdg_dir(const char* var, const char* fallback) {
    const char* v = std::getenv(var);
    // the spec says relative paths must be ignored
    fs::path base = (v && *v == '/') ? fs::path(v) : home_dir() / fallback;
    return base / "p2padga";
}

std::string errno_str(const char* what, const fs::path& p) {
    return std::string(what) + " " + p.string() + ": " + std::strerror(errno);
}

// Characters that must never be printed: C0, DEL, C1, and the Unicode
// controls that reorder or hide text (bidi embeddings/overrides/isolates,
// direction marks).
bool is_dangerous(char32_t c) {
    if (c < 0x20 || c == 0x7f || (c >= 0x80 && c <= 0x9f)) return true;
    if (c == 0x061c || c == 0x200e || c == 0x200f) return true;
    if (c >= 0x202a && c <= 0x202e) return true;
    if (c >= 0x2066 && c <= 0x2069) return true;
    if (c == 0xfeff) return true; // BOM / zero-width no-break space
    return false;
}

} // namespace

std::string format_bytes(std::int64_t n) {
    if (n < 0) n = 0;
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(n);
    int u = 0;
    while (v >= 1024.0 && u < 5) {
        v /= 1024.0;
        ++u;
    }
    if (u == 0) return std::to_string(n) + " B";
    return scaled(v, units[u]);
}

std::string format_rate(std::int64_t bps) {
    if (bps <= 0) return "-";
    return format_bytes(bps) + "/s";
}

std::string format_eta(std::int64_t s) {
    if (s < 0) return "-";
    if (s < 60) return std::to_string(s) + "s";
    if (s < 3600) return std::to_string(s / 60) + "m" + std::to_string(s % 60) + "s";
    if (s < 86400) return std::to_string(s / 3600) + "h" + std::to_string(s % 3600 / 60) + "m";
    if (s < 100 * 86400) return std::to_string(s / 86400) + "d" + std::to_string(s % 86400 / 3600) + "h";
    return "∞";
}

std::string format_percent(double f) {
    if (!(f >= 0)) f = 0; // also catches NaN
    if (f > 1) f = 1;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.1f%%", f * 100.0);
    return buf;
}

std::string format_ratio(double r) {
    if (!std::isfinite(r) || r < 0) return "-";
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", r);
    return buf;
}

std::string to_hex(const char* data, std::size_t len) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        auto b = static_cast<unsigned char>(data[i]);
        out += digits[b >> 4];
        out += digits[b & 0xf];
    }
    return out;
}

std::string trim(std::string_view s) {
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; };
    while (!s.empty() && ws(s.front())) s.remove_prefix(1);
    while (!s.empty() && ws(s.back())) s.remove_suffix(1);
    return std::string(s);
}

std::string expand_tilde(const std::string& path) {
    if (path == "~") return home_dir().string();
    if (path.rfind("~/", 0) == 0) return (home_dir() / path.substr(2)).string();
    return path;
}

void set_ascii_only(bool ascii_only) { g_ascii_only = ascii_only; }

std::wstring sanitize(std::string_view in) {
    std::wstring out;
    out.reserve(in.size());
    const auto* p = reinterpret_cast<const unsigned char*>(in.data());
    const auto* end = p + in.size();
    while (p < end) {
        char32_t c;
        int len;
        unsigned char b = *p;
        if (b < 0x80) { c = b; len = 1; }
        else if ((b & 0xe0) == 0xc0) { c = b & 0x1f; len = 2; }
        else if ((b & 0xf0) == 0xe0) { c = b & 0x0f; len = 3; }
        else if ((b & 0xf8) == 0xf0) { c = b & 0x07; len = 4; }
        else { out += L'�'; ++p; continue; }
        if (end - p < len) { out += L'�'; break; }
        bool ok = true;
        for (int i = 1; i < len; ++i) {
            if ((p[i] & 0xc0) != 0x80) { ok = false; break; }
            c = (c << 6) | (p[i] & 0x3f);
        }
        // reject overlong forms, surrogates and out-of-range values
        static const char32_t min_for_len[] = {0, 0, 0x80, 0x800, 0x10000};
        if (!ok || c < min_for_len[len] || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
            out += L'�';
            ++p;
            continue;
        }
        p += len;
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        if (is_dangerous(c)) c = 0xfffd;
        if (g_ascii_only && (c < 0x20 || c > 0x7e)) c = '?';
        out += static_cast<wchar_t>(c);
    }
    return out;
}

int display_width(const std::wstring& s) {
    int w = 0;
    for (wchar_t c : s) {
        int cw = ::wcwidth(c);
        w += cw < 0 ? 1 : cw;
    }
    return w;
}

std::wstring fit(const std::wstring& s, int width, bool right_align) {
    if (width <= 0) return L"";
    std::wstring out;
    int w = 0;
    const std::wstring ellipsis = g_ascii_only ? L"~" : L"…";
    if (display_width(s) <= width) {
        out = s;
        w = display_width(s);
    } else {
        for (wchar_t c : s) {
            int cw = ::wcwidth(c);
            if (cw < 0) cw = 1;
            if (w + cw > width - 1) break;
            out += c;
            w += cw;
        }
        out += ellipsis;
        w += 1;
    }
    std::wstring pad(static_cast<std::size_t>(width - w), L' ');
    return right_align ? pad + out : out + pad;
}

std::string to_utf8(const std::wstring& s) {
    std::string out;
    for (wchar_t wc : s) {
        auto c = static_cast<char32_t>(wc);
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) {
            out += static_cast<char>(0xc0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xe0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (c & 0x3f));
        } else if (c <= 0x10ffff) {
            out += static_cast<char>(0xf0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (c & 0x3f));
        }
    }
    return out;
}

fs::path config_dir() { return xdg_dir("XDG_CONFIG_HOME", ".config"); }
fs::path data_dir() { return xdg_dir("XDG_DATA_HOME", ".local/share"); }

std::optional<std::vector<char>> read_file(const fs::path& path, std::size_t max_size,
                                           std::string& err, bool no_follow) {
    int flags = O_RDONLY | O_CLOEXEC | O_NOCTTY | O_NONBLOCK;
    if (no_follow) flags |= O_NOFOLLOW;
    int fd = ::open(path.c_str(), flags);
    if (fd < 0) {
        err = errno_str("cannot open", path);
        return std::nullopt;
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        err = path.string() + " is not a regular file";
        ::close(fd);
        return std::nullopt;
    }
    if (static_cast<std::uint64_t>(st.st_size) > max_size) {
        err = path.string() + " is too large (" + format_bytes(st.st_size) + ", limit " +
              format_bytes(static_cast<std::int64_t>(max_size)) + ")";
        ::close(fd);
        return std::nullopt;
    }
    std::vector<char> buf;
    buf.reserve(static_cast<std::size_t>(st.st_size));
    char chunk[65536];
    for (;;) {
        ssize_t n = ::read(fd, chunk, sizeof chunk);
        if (n < 0) {
            if (errno == EINTR) continue;
            err = errno_str("cannot read", path);
            ::close(fd);
            return std::nullopt;
        }
        if (n == 0) break;
        // the file may grow after fstat; keep enforcing the limit
        if (buf.size() + static_cast<std::size_t>(n) > max_size) {
            err = path.string() + " is too large";
            ::close(fd);
            return std::nullopt;
        }
        buf.insert(buf.end(), chunk, chunk + n);
    }
    ::close(fd);
    return buf;
}

bool write_file_atomic(const fs::path& path, const std::vector<char>& data, std::string& err) {
    std::string tmpl = path.string() + ".tmp.XXXXXX";
    std::vector<char> name(tmpl.begin(), tmpl.end());
    name.push_back('\0');
    int fd = ::mkostemp(name.data(), O_CLOEXEC); // O_EXCL, mode 0600
    if (fd < 0) {
        err = errno_str("cannot create temp file for", path);
        return false;
    }
    const char* p = data.data();
    std::size_t left = data.size();
    while (left > 0) {
        ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            err = errno_str("cannot write", path);
            ::close(fd);
            ::unlink(name.data());
            return false;
        }
        p += n;
        left -= static_cast<std::size_t>(n);
    }
    if (::fsync(fd) != 0 || ::close(fd) != 0) {
        err = errno_str("cannot flush", path);
        ::unlink(name.data());
        return false;
    }
    if (::rename(name.data(), path.c_str()) != 0) {
        err = errno_str("cannot replace", path);
        ::unlink(name.data());
        return false;
    }
    // make the rename itself durable
    if (int dfd = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC); dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
    }
    return true;
}

bool ensure_private_dir(const fs::path& dir, std::string& err) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        err = "cannot create " + dir.string() + ": " + ec.message();
        return false;
    }
    struct stat st{};
    if (::lstat(dir.c_str(), &st) != 0) {
        err = errno_str("cannot stat", dir);
        return false;
    }
    if (!S_ISDIR(st.st_mode)) {
        err = dir.string() + " is not a directory (or is a symlink)";
        return false;
    }
    if (st.st_uid != ::getuid()) {
        err = dir.string() + " is owned by another user";
        return false;
    }
    if ((st.st_mode & 0077) != 0 && ::chmod(dir.c_str(), 0700) != 0) {
        err = errno_str("cannot make private:", dir);
        return false;
    }
    return true;
}

} // namespace util
