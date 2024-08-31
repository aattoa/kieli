#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include "test_interface.hpp"

static constexpr auto parse = libparse::test_parse_pattern;

#define TEST(name) UNITTEST("parse pattern: " name)
#define CHECK_SIMPLE_PARSE(string) CHECK_EQUAL(parse(string), (string))

TEST("literals")
{
    CHECK_SIMPLE_PARSE("5");
    CHECK_EQUAL(parse("5e3"), "5000");
    CHECK_EQUAL(parse("5.0"), "5");
    CHECK_EQUAL(parse("5.0e3"), "5000");
    CHECK_SIMPLE_PARSE("true");
    CHECK_SIMPLE_PARSE("false");
    CHECK_SIMPLE_PARSE("'x'");
    CHECK_SIMPLE_PARSE("'\\n'");
    CHECK_SIMPLE_PARSE("\"\"");
    CHECK_SIMPLE_PARSE("\"hello\"");
    CHECK_EQUAL(parse("\"hello,\tworld!\n\""), R"("hello,\tworld!\n")");
}

TEST("parenthesized")
{
    CHECK_SIMPLE_PARSE("()");
    CHECK_SIMPLE_PARSE("(x)");
    CHECK_SIMPLE_PARSE("(x, y)");
}

TEST("wildcard")
{
    CHECK_SIMPLE_PARSE("_");
    CHECK_SIMPLE_PARSE("____");
}

TEST("name")
{
    CHECK_SIMPLE_PARSE("x");
    CHECK_SIMPLE_PARSE("mut x");
}

TEST("constructor")
{
    CHECK_SIMPLE_PARSE("X::X");
    CHECK_SIMPLE_PARSE("X::X(a, b, c)");
    CHECK_SIMPLE_PARSE("X::X { a, b = 5, c }");
}

TEST("abbreviated constructor")
{
    CHECK_SIMPLE_PARSE("::X");
    CHECK_SIMPLE_PARSE("::X(a, b, c)");
    CHECK_SIMPLE_PARSE("::X { a, b = 5, c }");
}

TEST("slice")
{
    CHECK_SIMPLE_PARSE("[]");
    CHECK_SIMPLE_PARSE("[a]");
    CHECK_SIMPLE_PARSE("[a, [b, c], (d, e), f]");
}

TEST("alias")
{
    CHECK_SIMPLE_PARSE("(_, ___) as x");
    CHECK_SIMPLE_PARSE("(___, _) as mut x");
}

TEST("guarded")
{
    CHECK_SIMPLE_PARSE("_ if x");
    CHECK_SIMPLE_PARSE("x if x == y");
}
