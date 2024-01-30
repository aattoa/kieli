#include <libutl/common/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <cppunittest/unittest.hpp>

namespace {
    auto desugar(std::string&& string) -> std::string
    {
        auto [info, source] = kieli::test_info_and_source(std::move(string));
        auto const  module  = kieli::desugar(kieli::parse(source, info), info);
        std::string output;
        for (ast::Definition const& definition : module.definitions) {
            ast::format_to(definition, output);
        }
        return output;
    }
} // namespace

#define TEST(name) UNITTEST("libdesugar: " name)

#define CHECK_SIMPLE_DESUGAR(string) CHECK_EQUAL(desugar(string), (string))

// TODO: struct initializer expressions

TEST("block expression")
{
    CHECK_EQUAL(desugar("fn f() {}"), "fn f() { () }");
    CHECK_EQUAL(desugar("fn f() { 5 }"), "fn f() { 5 }");
    CHECK_EQUAL(desugar("fn f() { 5; }"), "fn f() { 5; () }");
    CHECK_EQUAL(desugar("fn f() { 5; 10 }"), "fn f() { 5; 10 }");
    CHECK_EQUAL(desugar("fn f() { 5; 10; }"), "fn f() { 5; 10; () }");
}

TEST("function body normalization")
{
    CHECK_EQUAL("fn f() { 5 }", desugar("fn f() { 5 }"));
    CHECK_EQUAL("fn f() { 5 }", desugar("fn f() = 5"));
    CHECK_EQUAL("fn f() { 5 }", desugar("fn f() = { 5 }"));
}

TEST("operator precedence")
{
    /*
        precedence table:
        "*", "/", "%"
        "+", "-"
        "?=", "!="
        "<", "<=", ">=", ">"
        "&&", "||"
        ":=", "+=", "*=", "/=", "%="
     */
    CHECK_EQUAL(
        desugar("fn f() { (a * b + c) + (d + e * f) }"),
        "fn f() { (((a * b) + c) + (d + (e * f))) }");
    CHECK_EQUAL(
        desugar("fn f() { a <$> b && c <= d ?= e + f / g }"),
        "fn f() { (a <$> (b && (c <= (d ?= (e + (f / g)))))) }");
    CHECK_EQUAL(
        desugar("fn f() { a / b + c ?= d <= e && f <$> g }"),
        "fn f() { ((((((a / b) + c) ?= d) <= e) && f) <$> g) }");
    CHECK_EQUAL(desugar("fn f() { a + b && c }"), "fn f() { ((a + b) && c) }");
    CHECK_EQUAL(desugar("fn f() { a %% c % d ?= e }"), "fn f() { (a %% ((c % d) ?= e)) }");
    CHECK_EQUAL(desugar("fn f() { a + b + c + d }"), "fn f() { (((a + b) + c) + d) }");
}

TEST("while loop expression")
{
    CHECK_EQUAL(
        desugar("fn f() { while x { y } }"), "fn f() { loop { if x { y } else break () } }");
    CHECK_EQUAL(
        desugar("fn f() { while let x = y { z } }"),
        "fn f() { loop { match y { immut x -> { z } _ -> break () } } }");
}

TEST("conditional expression")
{
    CHECK_EQUAL(desugar("fn f() { if x { y } }"), "fn f() { if x { y } else () }");
    CHECK_EQUAL(desugar("fn f() { if x { y } else { z } }"), "fn f() { if x { y } else { z } }");
    CHECK_EQUAL(
        desugar("fn f() { if let x = y { z } }"),
        "fn f() { match y { immut x -> { z } _ -> () } }");
    CHECK_EQUAL(
        desugar("fn f() { if let a = b { c } else { d } }"),
        "fn f() { match b { immut a -> { c } _ -> { d } } }");
}

TEST("discard expression")
{
    CHECK_EQUAL(desugar("fn f() { discard x; }"), "fn f() { { let _ = x; () }; () }");
}

TEST("struct")
{
    CHECK_SIMPLE_DESUGAR("struct S = a: Int, b: Float");
    CHECK_SIMPLE_DESUGAR("struct S[A, B] = a: A, b: B");
}

TEST("enum")
{
    CHECK_SIMPLE_DESUGAR("enum E = aaa | bbb(Int) | ccc(Float, Char)");
    CHECK_SIMPLE_DESUGAR("enum Option[T] = none | some(T)");
}

TEST("alias")
{
    CHECK_SIMPLE_DESUGAR("alias T = U");
    CHECK_SIMPLE_DESUGAR("alias A[B] = (B, B)");
}
