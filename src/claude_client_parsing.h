#pragma once
#include <string>
#include <vector>

// Pure-C++ string helpers for the Cardputer Claude client. No Arduino headers.
// Host-testable with plain g++.
namespace claude_parsing {

// True if the line starts (with no leading whitespace) with the literal "[error:".
bool is_error_line(const std::string& line);

// Word-wrap `text` to lines no longer than `cols` characters.
// Treats '\n' as a hard line break. Breaks on the last whitespace before the
// limit when possible; otherwise hard-breaks at `cols`.
// Empty input returns an empty vector.
std::vector<std::string> wrap_text(const std::string& text, std::size_t cols);

// If buf.size() > cap, return the last `keep` characters of buf; otherwise
// return buf unchanged. `keep` should be <= cap; behaviour for keep > buf size
// is "return buf".
std::string truncate_keep_tail(const std::string& buf, std::size_t cap, std::size_t keep);

}  // namespace claude_parsing
