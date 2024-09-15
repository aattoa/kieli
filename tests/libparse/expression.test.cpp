#include <cppunittest/unittest.hpp>
#include <libutl/utilities.hpp>
#include "test_interface.hpp"

static constexpr auto parse = libparse::test_parse_expression;

#define TEST(name) UNITTEST("parse expression: " name)
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
    CHECK_SIMPLE_PARSE("\"hello,\\tworld!\\n\"");
}

TEST("parenthesized")
{
    CHECK_SIMPLE_PARSE("()");
    CHECK_SIMPLE_PARSE("(5)");
    CHECK_SIMPLE_PARSE("(5, 3)");
}

TEST("array literal")
{
    CHECK_SIMPLE_PARSE("[]");
    CHECK_SIMPLE_PARSE("[5]");
    CHECK_SIMPLE_PARSE("[5, 3]");
}

TEST("path")
{
    CHECK_SIMPLE_PARSE("x");
    CHECK_SIMPLE_PARSE("x[]");
    CHECK_SIMPLE_PARSE("x[A, B]");
    CHECK_SIMPLE_PARSE("_x");
    CHECK_SIMPLE_PARSE("x::y");
    CHECK_SIMPLE_PARSE("x::_y");
    CHECK_SIMPLE_PARSE("x[]::y");
    CHECK_SIMPLE_PARSE("x[]::_y");
    CHECK_SIMPLE_PARSE("x[A, B]::y");
    CHECK_SIMPLE_PARSE("x[A, B]::_y");
    CHECK_SIMPLE_PARSE("global::x");
    CHECK_SIMPLE_PARSE("global::_x");
    CHECK_SIMPLE_PARSE("global::x::y");
    CHECK_SIMPLE_PARSE("global::x::_y");
    CHECK_SIMPLE_PARSE("global::x[]::y");
    CHECK_SIMPLE_PARSE("global::x[]::_y");
    CHECK_SIMPLE_PARSE("global::x[A, B]::y");
    CHECK_SIMPLE_PARSE("global::x[A, B]::y[]");
    CHECK_SIMPLE_PARSE("global::x[A, B]::y[A, B]");
    CHECK_SIMPLE_PARSE("global::x[A, B]::_y");
    CHECK_SIMPLE_PARSE("typeof(x)::x");
    CHECK_SIMPLE_PARSE("typeof(x)::_x");
    CHECK_SIMPLE_PARSE("typeof(x)::x::y");
    CHECK_SIMPLE_PARSE("typeof(x)::x::_y");
    CHECK_SIMPLE_PARSE("typeof(x)::x[]::y");
    CHECK_SIMPLE_PARSE("typeof(x)::x[]::_y");
    CHECK_SIMPLE_PARSE("typeof(x)::x[A, B]::y");
    CHECK_SIMPLE_PARSE("typeof(x)::x[A, B]::y[]");
    CHECK_SIMPLE_PARSE("typeof(x)::x[A, B]::y[A, B]");
    CHECK_SIMPLE_PARSE("typeof(x)::x[A, B]::_y");
}

TEST("block")
{
    CHECK_SIMPLE_PARSE("{}");
    CHECK_SIMPLE_PARSE("{ x }");
    CHECK_EQUAL(
        parse("{ x; y }"),
        "{\n"
        "    x;\n"
        "    y\n"
        "}");
    CHECK_EQUAL(
        parse("{ a; { b; c; }; d; { e; f } }"),
        "{\n"
        "    a;\n"
        "    {\n"
        "        b;\n"
        "        c;\n"
        "    };\n"
        "    d;\n"
        "    {\n"
        "        e;\n"
        "        f\n"
        "    }\n"
        "}");
}

TEST("function call")
{
    CHECK_SIMPLE_PARSE("f()");
    CHECK_SIMPLE_PARSE("f(x, y)");
    CHECK_SIMPLE_PARSE("a::b()");
    CHECK_SIMPLE_PARSE("a::b(x, y)");
    CHECK_SIMPLE_PARSE("(a.b)()");
    CHECK_SIMPLE_PARSE("(a.b)(x, y)");
}

TEST("method call")
{
    CHECK_SIMPLE_PARSE("a.b()");
    CHECK_SIMPLE_PARSE("a.b(x, y)");
    CHECK_SIMPLE_PARSE("a::b.c()");
    CHECK_SIMPLE_PARSE("a::b.c(x, y)");
}

TEST("struct initializer")
{
    CHECK_SIMPLE_PARSE("S { x = 10 }");
    CHECK_SIMPLE_PARSE("S { x = 10, y = \"hello\" }");
    CHECK_SIMPLE_PARSE("A::B { x = 10 }");
    CHECK_SIMPLE_PARSE("typeof(x)::T { x = 10 }");
}

TEST("tuple initializer")
{
    CHECK_SIMPLE_PARSE("S(10)");
    CHECK_SIMPLE_PARSE("S(10, \"hello\")");
    CHECK_SIMPLE_PARSE("A::B(10)");
    CHECK_SIMPLE_PARSE("typeof(x)::T(10)");
}

TEST("binary operator application")
{
    CHECK_SIMPLE_PARSE("a * b");
    CHECK_SIMPLE_PARSE("a <$> b");
    CHECK_SIMPLE_PARSE("a * b + c");
    CHECK_SIMPLE_PARSE("a *** (a <=> b) +++ c");
}

TEST("struct field access")
{
    CHECK_SIMPLE_PARSE("a.b");
    CHECK_SIMPLE_PARSE("a.b.c");
}

TEST("tuple field access")
{
    CHECK_SIMPLE_PARSE("x.0");
    CHECK_SIMPLE_PARSE("x.0.1");
}

TEST("array field access")
{
    CHECK_SIMPLE_PARSE("x.[y]");
    CHECK_SIMPLE_PARSE("x.[y].[z]");
}

TEST("conditional")
{
    CHECK_EQUAL(
        parse("if a { b }"),
        "if a {\n"
        "    b\n"
        "}");
    CHECK_EQUAL(
        parse("if a { b } else { c }"),
        "if a {\n"
        "    b\n"
        "}\n"
        "else {\n"
        "    c\n"
        "}");
    CHECK_EQUAL(
        parse("if a { b } elif c { d } elif e { f } else { g }"),
        "if a {\n"
        "    b\n"
        "}\n"
        "elif c {\n"
        "    d\n"
        "}\n"
        "elif e {\n"
        "    f\n"
        "}\n"
        "else {\n"
        "    g\n"
        "}");
    CHECK_EQUAL(
        parse("if let a = b { c }"),
        "if let a = b {\n"
        "    c\n"
        "}");
    CHECK_EQUAL(
        parse("if let a = b { c } else { d }"),
        "if let a = b {\n"
        "    c\n"
        "}\n"
        "else {\n"
        "    d\n"
        "}");
    CHECK_EQUAL(
        parse("if let a = b { c } elif let d = e { f } else { g }"),
        "if let a = b {\n"
        "    c\n"
        "}\n"
        "elif let d = e {\n"
        "    f\n"
        "}\n"
        "else {\n"
        "    g\n"
        "}");
}

TEST("match")
{
    CHECK_EQUAL(
        parse("match a { b -> c d -> e }"),
        "match a {\n"
        "    b -> c\n"
        "    d -> e\n"
        "}");
    CHECK_EQUAL(
        parse("match a { b, c -> d; (e, f) -> g }"),
        "match a {\n"
        "    b, c -> d;\n"
        "    (e, f) -> g\n"
        "}");
}

TEST("type cast")
{
    CHECK_SIMPLE_PARSE("x as X");
    CHECK_SIMPLE_PARSE("a as B as C");
}

TEST("type ascription")
{
    CHECK_SIMPLE_PARSE("x: X");
    CHECK_SIMPLE_PARSE("a: B: C");
}

TEST("let binding")
{
    CHECK_SIMPLE_PARSE("let x = y");
    CHECK_SIMPLE_PARSE("let x: T = y");
    CHECK_SIMPLE_PARSE("let (a, b) = x");
    CHECK_SIMPLE_PARSE("let (a, b): (A, B) = x");
}

TEST("type alias")
{
    CHECK_SIMPLE_PARSE("alias T = I32");
}

TEST("plain loop")
{
    CHECK_SIMPLE_PARSE("loop {}");
}

TEST("while loop")
{
    CHECK_SIMPLE_PARSE("while x { y }");
    CHECK_SIMPLE_PARSE("while let x = y { z }");
}

TEST("for loop")
{
    CHECK_SIMPLE_PARSE("for x in xs {}");
    CHECK_SIMPLE_PARSE("for (x, y) in [(10, 'x'), (20, 'y')] {}");
}

TEST("loop directives")
{
    CHECK_SIMPLE_PARSE("continue");
    CHECK_SIMPLE_PARSE("break");
    CHECK_SIMPLE_PARSE("break 5");
}

TEST("discard")
{
    CHECK_SIMPLE_PARSE("discard x");
    CHECK_SIMPLE_PARSE("discard (x)");
    CHECK_SIMPLE_PARSE("discard {}");
    CHECK_SIMPLE_PARSE("discard { x }");
}

TEST("ret")
{
    CHECK_SIMPLE_PARSE("ret");
    CHECK_SIMPLE_PARSE("ret x");
}

TEST("reference")
{
    CHECK_SIMPLE_PARSE("&x");
    CHECK_SIMPLE_PARSE("&mut x");
    CHECK_SIMPLE_PARSE("&x.y");
    CHECK_SIMPLE_PARSE("&mut x.y");
}

TEST("sizeof")
{
    CHECK_SIMPLE_PARSE("sizeof(T)");
    CHECK_SIMPLE_PARSE("sizeof((A, B))");
    CHECK_SIMPLE_PARSE("sizeof(a::b::C)");
}

TEST("dereference")
{
    CHECK_SIMPLE_PARSE("*x");
    CHECK_SIMPLE_PARSE("*x.y");
}

TEST("mv")
{
    CHECK_SIMPLE_PARSE("mv x");
    CHECK_SIMPLE_PARSE("mv x.y");
    CHECK_SIMPLE_PARSE("mv x.[y]");
}
