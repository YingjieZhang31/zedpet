#include "claude_client_parsing.h"

namespace claude_parsing {

bool is_error_line(const std::string& line) {
    static const std::string prefix = "[error:";
    return line.compare(0, prefix.size(), prefix) == 0;
}

static void push_wrapped(std::vector<std::string>& out, const std::string& s, std::size_t cols) {
    if (s.empty()) {
        out.emplace_back("");
        return;
    }
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t remaining = s.size() - i;
        if (remaining <= cols) {
            out.push_back(s.substr(i));
            return;
        }
        // Try to break at last whitespace within [i, i+cols].
        // If s[i+cols] is a space, we can emit exactly cols chars and skip it.
        // Otherwise scan backwards for a space within the window.
        std::size_t break_at = std::string::npos;
        std::size_t window_end = i + cols;  // first index past the cols window
        // Check position i+cols itself (would let us emit exactly cols chars)
        if (window_end < s.size() && (s[window_end] == ' ' || s[window_end] == '\t')) {
            break_at = window_end;
        } else {
            for (std::size_t j = window_end; j > i; --j) {
                if (s[j - 1] == ' ' || s[j - 1] == '\t') {
                    break_at = j - 1;
                    break;
                }
            }
        }
        if (break_at == std::string::npos) {
            out.push_back(s.substr(i, cols));
            i += cols;
        } else {
            out.push_back(s.substr(i, break_at - i));
            i = break_at + 1;  // skip the whitespace
        }
    }
}

std::vector<std::string> wrap_text(const std::string& text, std::size_t cols) {
    std::vector<std::string> out;
    if (text.empty() || cols == 0) return out;

    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t nl = text.find('\n', start);
        std::string segment = (nl == std::string::npos)
            ? text.substr(start)
            : text.substr(start, nl - start);
        push_wrapped(out, segment, cols);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

std::string truncate_keep_tail(const std::string& buf, std::size_t cap, std::size_t keep) {
    if (buf.size() <= cap) return buf;
    if (keep >= buf.size()) return buf;
    return buf.substr(buf.size() - keep);
}

}  // namespace claude_parsing
