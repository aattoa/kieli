#include <libutl/common/utilities.hpp>
#include <libparse/test_interface.hpp>
#include <catch2/catch_test_macros.hpp>

namespace { constexpr auto parse = libparse::test_parse_pattern; }

#define TEST(name) TEST_CASE("parse-pattern " name, "[libparse][pattern]") // NOLINT
#define REQUIRE_SIMPLE_PARSE(string) REQUIRE(parse(string) == (string))


TEST("literals") {
    REQUIRE_SIMPLE_PARSE("5");
    REQUIRE_SIMPLE_PARSE("5e3");
    REQUIRE_SIMPLE_PARSE("5.0");
    REQUIRE_SIMPLE_PARSE("5.0e3");
    REQUIRE_SIMPLE_PARSE("true");
    REQUIRE_SIMPLE_PARSE("false");
    REQUIRE_SIMPLE_PARSE("'x'");
    REQUIRE_SIMPLE_PARSE("'\n'");
    REQUIRE_SIMPLE_PARSE("\"\"");
    REQUIRE_SIMPLE_PARSE("\"hello\"");
    REQUIRE_SIMPLE_PARSE("\"hello,\tworld!\n\"");
}

TEST("parenthesized") {
    REQUIRE_SIMPLE_PARSE("()");
    REQUIRE_SIMPLE_PARSE("(x)");
    REQUIRE_SIMPLE_PARSE("(x, y)");
}

TEST("wildcard") {
    REQUIRE_SIMPLE_PARSE("_");
    REQUIRE_SIMPLE_PARSE("___");
}

TEST("name") {
    REQUIRE_SIMPLE_PARSE("x");
    REQUIRE_SIMPLE_PARSE("mut x");
}

TEST("constructor") {
    REQUIRE_SIMPLE_PARSE("X::x");
    REQUIRE_SIMPLE_PARSE("X::x(a, b, c)");
}

TEST("abbreviated constructor") {
    REQUIRE_SIMPLE_PARSE("::x");
    REQUIRE_SIMPLE_PARSE("::x(a, b, c)");
}

TEST("slice") {
    REQUIRE_SIMPLE_PARSE("[]");
    REQUIRE_SIMPLE_PARSE("[a]");
    REQUIRE_SIMPLE_PARSE("[a, [b, c], (d, e), f]");
}

TEST("alias") {
    REQUIRE_SIMPLE_PARSE("(_, ___) as x");
    REQUIRE_SIMPLE_PARSE("(___, _) as mut x");
}

TEST("guarded") {
    REQUIRE_SIMPLE_PARSE("_ if x");
    REQUIRE_SIMPLE_PARSE("x if x == y");
}
