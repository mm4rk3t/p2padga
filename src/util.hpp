#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace util {

namespace fs = std::filesystem;

// Human-readable formatting.
std::string format_bytes(std::int64_t n);
std::string format_rate(std::int64_t bytes_per_second);
std::string format_eta(std::int64_t seconds);
std::string format_percent(double fraction);
std::string format_ratio(double ratio);

// Lowercase hex encoding of raw bytes.
std::string to_hex(const char* data, std::size_t len);

// Trim ASCII whitespace from both ends.
std::string trim(std::string_view s);

// Expand a leading "~" or "~/" to the home directory.
std::string expand_tilde(const std::string& path);

// Untrusted text (torrent names, file names from peers, tracker messages)
// must never reach the terminal raw: a crafted name could carry escape
// sequences or bidi overrides. sanitize() decodes UTF-8 (invalid bytes
// become U+FFFD) and replaces every control character (C0, DEL, C1, bidi
// controls) so the result is safe to print. In ASCII mode (non-UTF-8
// locale) anything outside printable ASCII becomes '?'.
void set_ascii_only(bool ascii_only);
std::wstring sanitize(std::string_view untrusted);

// Terminal columns taken by a sanitized string.
int display_width(const std::wstring& s);

// Truncate (ending in an ellipsis) or pad a sanitized string to exactly
// `width` columns.
std::wstring fit(const std::wstring& s, int width, bool right_align = false);

// Wide string (from keyboard input) back to UTF-8.
std::string to_utf8(const std::wstring& s);

// XDG base directories for p2padga ($XDG_CONFIG_HOME/p2padga,
// $XDG_DATA_HOME/p2padga).
fs::path config_dir();
fs::path data_dir();

// Read a whole regular file, refusing anything larger than max_size or
// anything that isn't a regular file. With no_follow, a symlink at `path`
// is refused too (for our own state files).
std::optional<std::vector<char>> read_file(const fs::path& path, std::size_t max_size,
                                           std::string& err, bool no_follow = false);

// Atomically replace `path`: write a 0600 temp file created with O_EXCL in
// the same directory, fsync it, rename it over the target.
bool write_file_atomic(const fs::path& path, const std::vector<char>& data, std::string& err);

// Create `dir` (and parents) with mode 0700 and make sure it's a real
// directory owned by us that nobody else can write to.
bool ensure_private_dir(const fs::path& dir, std::string& err);

} // namespace util
