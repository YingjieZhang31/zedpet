#include "claude_client_parsing.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace claude_parsing;

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { ++failures; std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define EQ(a, b) do { \
    auto _av = (a); auto _bv = (b); \
    if (!(_av == _bv)) { ++failures; std::fprintf(stderr, "FAIL %s:%d expected %s == %s\n", __FILE__, __LINE__, #a, #b); } \
} while (0)

static void test_is_error_line() {
    CHECK(is_error_line("[error: boom]"));
    CHECK(is_error_line("[error: anything goes"));
    CHECK(!is_error_line("hello world"));
    CHECK(!is_error_line(""));
    CHECK(!is_error_line("error: not bracketed"));
    CHECK(!is_error_line(" [error: leading space not allowed"));
}

static void test_wrap_text_short() {
    auto lines = wrap_text("hello", 40);
    EQ(lines.size(), (size_t)1);
    EQ(lines[0], std::string("hello"));
}

static void test_wrap_text_break_on_whitespace() {
    auto lines = wrap_text("hello world this is a test", 10);
    EQ(lines.size(), (size_t)3);
    EQ(lines[0], std::string("hello"));
    EQ(lines[1], std::string("world this"));
    EQ(lines[2], std::string("is a test"));
}

static void test_wrap_text_hard_break_long_word() {
    auto lines = wrap_text("abcdefghijklmnop", 5);
    EQ(lines.size(), (size_t)4);
    EQ(lines[0], std::string("abcde"));
    EQ(lines[1], std::string("fghij"));
    EQ(lines[2], std::string("klmno"));
    EQ(lines[3], std::string("p"));
}

static void test_wrap_text_preserves_explicit_newlines() {
    auto lines = wrap_text("line one\nline two\nline three", 40);
    EQ(lines.size(), (size_t)3);
    EQ(lines[0], std::string("line one"));
    EQ(lines[1], std::string("line two"));
    EQ(lines[2], std::string("line three"));
}

static void test_wrap_text_empty_returns_empty() {
    auto lines = wrap_text("", 40);
    EQ(lines.size(), (size_t)0);
}

static void test_truncate_keep_tail_no_op_when_under_cap() {
    std::string s = "short";
    EQ(truncate_keep_tail(s, 100, 50), std::string("short"));
}

static void test_truncate_keep_tail_keeps_last_n() {
    std::string s(5000, 'x');
    s += "TAIL";
    std::string out = truncate_keep_tail(s, 4096, 3000);
    EQ(out.size(), (size_t)3000);
    EQ(out.substr(out.size() - 4), std::string("TAIL"));
}

static void test_truncate_keep_tail_exactly_at_cap_is_no_op() {
    std::string s(4096, 'y');
    EQ(truncate_keep_tail(s, 4096, 3000).size(), (size_t)4096);
}

int main() {
    test_is_error_line();
    test_wrap_text_short();
    test_wrap_text_break_on_whitespace();
    test_wrap_text_hard_break_long_word();
    test_wrap_text_preserves_explicit_newlines();
    test_wrap_text_empty_returns_empty();
    test_truncate_keep_tail_no_op_when_under_cap();
    test_truncate_keep_tail_keeps_last_n();
    test_truncate_keep_tail_exactly_at_cap_is_no_op();

    if (failures == 0) {
        std::printf("OK: all parsing tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
}
