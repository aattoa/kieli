#include <catch2/catch_test_macros.hpp>
#include <libparse2/test_interface.hpp>
#include <libutl/common/utilities.hpp>

namespace {
    constexpr auto parse = libparse2::test_parse_type;
}

#define TEST(name) TEST_CASE("parse-type " name, "[libparse][type]") // NOLINT

#define REQUIRE_SIMPLE_PARSE(string) REQUIRE(parse(string) == (string))

TEST("built in types")
{
    REQUIRE_SIMPLE_PARSE("I8");
    REQUIRE_SIMPLE_PARSE("I16");
    REQUIRE_SIMPLE_PARSE("I32");
    REQUIRE_SIMPLE_PARSE("I64");
    REQUIRE_SIMPLE_PARSE("U8");
    REQUIRE_SIMPLE_PARSE("U16");
    REQUIRE_SIMPLE_PARSE("U32");
    REQUIRE_SIMPLE_PARSE("U64");
    REQUIRE_SIMPLE_PARSE("Float");
    REQUIRE_SIMPLE_PARSE("Char");
    REQUIRE_SIMPLE_PARSE("Bool");
    REQUIRE_SIMPLE_PARSE("String");
}

TEST("parenthesized")
{
    REQUIRE_SIMPLE_PARSE("()");
    REQUIRE_SIMPLE_PARSE("(I32)");
    REQUIRE_SIMPLE_PARSE("(I32, (), String)");
}

TEST("wildcard")
{
    REQUIRE_SIMPLE_PARSE("_");
    // TODO: REQUIRE_SIMPLE_PARSE("____");
}

TEST("self")
{
    REQUIRE_SIMPLE_PARSE("Self");
}

TEST("typename")
{
    REQUIRE_SIMPLE_PARSE("T");
    REQUIRE_SIMPLE_PARSE("a::B");
    REQUIRE_SIMPLE_PARSE("A::B");
    REQUIRE_SIMPLE_PARSE("typeof(x)::B");
    REQUIRE_SIMPLE_PARSE("typeof(x)::B[I32]::C");
}

TEST("template application")
{
    REQUIRE_SIMPLE_PARSE("T[]");
    REQUIRE_SIMPLE_PARSE("T[I32]");
    REQUIRE_SIMPLE_PARSE("a::B[]");
    REQUIRE_SIMPLE_PARSE("a::B[I32]");
    REQUIRE_SIMPLE_PARSE("A::B[]");
    REQUIRE_SIMPLE_PARSE("A::B[I32]");
    REQUIRE_SIMPLE_PARSE("typeof(x)::B[]");
    REQUIRE_SIMPLE_PARSE("typeof(x)::B[I32]");
    REQUIRE_SIMPLE_PARSE("typeof(x)::B[I32]::C[]");
    REQUIRE_SIMPLE_PARSE("typeof(x)::B[I32]::C[I32]");
}

TEST("array")
{
    REQUIRE_SIMPLE_PARSE("[T; n]");
    REQUIRE_SIMPLE_PARSE("[std::Vector[I32]; 5]");
}

TEST("slice")
{
    REQUIRE_SIMPLE_PARSE("[T]");
    REQUIRE_SIMPLE_PARSE("[std::Vector[I32]]");
}

TEST("function")
{
    REQUIRE_SIMPLE_PARSE("fn(): ()");
    REQUIRE_SIMPLE_PARSE("fn(): fn(): fn(): ()");
    REQUIRE_SIMPLE_PARSE("fn(I32): U32");
    REQUIRE_SIMPLE_PARSE("fn(T): (T, T, T)");
}

TEST("typeof")
{
    REQUIRE_SIMPLE_PARSE("typeof(x)");
    REQUIRE_SIMPLE_PARSE("typeof((x, y))");
    REQUIRE_SIMPLE_PARSE("typeof(\"hello\")");
}

TEST("inst")
{
    REQUIRE_SIMPLE_PARSE("inst Num");
    REQUIRE_SIMPLE_PARSE("inst Convertible_to[I32]");
    REQUIRE_SIMPLE_PARSE("inst Num + Convertible_to[I32]");
}

TEST("reference")
{
    REQUIRE_SIMPLE_PARSE("&T");
    REQUIRE_SIMPLE_PARSE("&Self");
    REQUIRE_SIMPLE_PARSE("&std::Vector");
    REQUIRE_SIMPLE_PARSE("&mut T");
    REQUIRE_SIMPLE_PARSE("&mut Self");
    REQUIRE_SIMPLE_PARSE("&mut std::Vector");
}

TEST("pointer")
{
    REQUIRE_SIMPLE_PARSE("*T");
    REQUIRE_SIMPLE_PARSE("*Self");
    REQUIRE_SIMPLE_PARSE("*std::Vector");
    REQUIRE_SIMPLE_PARSE("*mut T");
    REQUIRE_SIMPLE_PARSE("*mut Self");
    REQUIRE_SIMPLE_PARSE("*mut std::Vector");
}
