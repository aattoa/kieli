#include <libutl/utilities.hpp>
#include <libparse/test_interface.hpp>
#include <cppunittest/unittest.hpp>

namespace {
    constexpr auto parse = libparse::test_parse_type;
}

#define TEST(name) UNITTEST("parse type: " name)

#define CHECK_SIMPLE_PARSE(string) CHECK_EQUAL(parse(string), (string))

TEST("built in types")
{
    CHECK_SIMPLE_PARSE("I8");
    CHECK_SIMPLE_PARSE("I16");
    CHECK_SIMPLE_PARSE("I32");
    CHECK_SIMPLE_PARSE("I64");
    CHECK_SIMPLE_PARSE("U8");
    CHECK_SIMPLE_PARSE("U16");
    CHECK_SIMPLE_PARSE("U32");
    CHECK_SIMPLE_PARSE("U64");
    CHECK_SIMPLE_PARSE("Float");
    CHECK_SIMPLE_PARSE("Char");
    CHECK_SIMPLE_PARSE("Bool");
    CHECK_SIMPLE_PARSE("String");
}

TEST("parenthesized")
{
    CHECK_SIMPLE_PARSE("()");
    CHECK_SIMPLE_PARSE("(I32)");
    CHECK_SIMPLE_PARSE("(I32, (), String)");
}

TEST("wildcard")
{
    CHECK_SIMPLE_PARSE("_");
    CHECK_SIMPLE_PARSE("____");
}

TEST("self")
{
    CHECK_SIMPLE_PARSE("Self");
}

TEST("typename")
{
    CHECK_SIMPLE_PARSE("T");
    CHECK_SIMPLE_PARSE("a::B");
    CHECK_SIMPLE_PARSE("A::B");
    CHECK_SIMPLE_PARSE("typeof(x)::B");
    CHECK_SIMPLE_PARSE("typeof(x)::B[I32]::C");
}

TEST("template application")
{
    CHECK_SIMPLE_PARSE("T[]");
    CHECK_SIMPLE_PARSE("T[I32]");
    CHECK_SIMPLE_PARSE("a::B[]");
    CHECK_SIMPLE_PARSE("a::B[I32]");
    CHECK_SIMPLE_PARSE("A::B[]");
    CHECK_SIMPLE_PARSE("A::B[I32]");
    CHECK_SIMPLE_PARSE("typeof(x)::B[]");
    CHECK_SIMPLE_PARSE("typeof(x)::B[I32]");
    CHECK_SIMPLE_PARSE("typeof(x)::B[I32]::C[]");
    CHECK_SIMPLE_PARSE("typeof(x)::B[I32]::C[I32]");
}

TEST("array")
{
    CHECK_SIMPLE_PARSE("[T; n]");
    CHECK_SIMPLE_PARSE("[std::Vector[I32]; 5]");
}

TEST("slice")
{
    CHECK_SIMPLE_PARSE("[T]");
    CHECK_SIMPLE_PARSE("[std::Vector[I32]]");
}

TEST("function")
{
    CHECK_SIMPLE_PARSE("fn(): ()");
    CHECK_SIMPLE_PARSE("fn(): fn(): fn(): ()");
    CHECK_SIMPLE_PARSE("fn(I32): U32");
    CHECK_SIMPLE_PARSE("fn(T): (T, T, T)");
}

TEST("typeof")
{
    CHECK_SIMPLE_PARSE("typeof(x)");
    CHECK_SIMPLE_PARSE("typeof((x, y))");
    CHECK_SIMPLE_PARSE("typeof(\"hello\")");
}

TEST("impl")
{
    CHECK_SIMPLE_PARSE("impl Num");
    CHECK_SIMPLE_PARSE("impl Convertible_to[I32]");
    CHECK_SIMPLE_PARSE("impl Num + Convertible_to[I32]");
}

TEST("reference")
{
    CHECK_SIMPLE_PARSE("&T");
    CHECK_SIMPLE_PARSE("&Self");
    CHECK_SIMPLE_PARSE("&std::Vector");
    CHECK_SIMPLE_PARSE("&mut T");
    CHECK_SIMPLE_PARSE("&mut Self");
    CHECK_SIMPLE_PARSE("&mut std::Vector");
}

TEST("pointer")
{
    CHECK_SIMPLE_PARSE("*T");
    CHECK_SIMPLE_PARSE("*Self");
    CHECK_SIMPLE_PARSE("*std::Vector");
    CHECK_SIMPLE_PARSE("*mut T");
    CHECK_SIMPLE_PARSE("*mut Self");
    CHECK_SIMPLE_PARSE("*mut std::Vector");
}
