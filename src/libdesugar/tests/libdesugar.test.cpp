#include <libutl/common/utilities.hpp>
#include <libparse2/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    auto desugar(std::string&& string) -> std::string
    {
        auto [info, source] = kieli::test_info_and_source(std::move(string));
        auto const  module  = kieli::desugar(kieli::parse2(source, info), info);
        std::string output;
        for (ast::Definition const& definition : module.definitions) {
            ast::format_to(definition, output);
        }
        return output;
    }
} // namespace

#define TEST(name) TEST_CASE("desugar " name, "[libdesugar]") // NOLINT
#define REQUIRE_SIMPLE_DESUGAR(string) REQUIRE(desugar(string) == (string))

// TODO: struct initializer expressions

TEST("block expression")
{
    REQUIRE(desugar("fn f() {}") == "fn f() { () }");
    REQUIRE(desugar("fn f() { 5 }") == "fn f() { 5 }");
    REQUIRE(desugar("fn f() { 5; }") == "fn f() { 5; () }");
    REQUIRE(desugar("fn f() { 5; 10 }") == "fn f() { 5; 10 }");
    REQUIRE(desugar("fn f() { 5; 10; }") == "fn f() { 5; 10; () }");
}

TEST("function body normalization")
{
    REQUIRE("fn f() { 5 }" == desugar("fn f() { 5 }"));
    REQUIRE("fn f() { 5 }" == desugar("fn f() = 5"));
    REQUIRE("fn f() { 5 }" == desugar("fn f() = { 5 }"));
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
    REQUIRE(
        desugar("fn f() { (a * b + c) + (d + e * f) }")
        == "fn f() { (((a * b) + c) + (d + (e * f))) }");
    REQUIRE(
        desugar("fn f() { a <$> b && c <= d ?= e + f / g }")
        == "fn f() { (a <$> (b && (c <= (d ?= (e + (f / g)))))) }");
    REQUIRE(
        desugar("fn f() { a / b + c ?= d <= e && f <$> g }")
        == "fn f() { ((((((a / b) + c) ?= d) <= e) && f) <$> g) }");
    REQUIRE(desugar("fn f() { a + b && c }") == "fn f() { ((a + b) && c) }");
    REQUIRE(desugar("fn f() { a %% c % d ?= e }") == "fn f() { (a %% ((c % d) ?= e)) }");
    REQUIRE(desugar("fn f() { a + b + c + d }") == "fn f() { (((a + b) + c) + d) }");
}

TEST("while loop expression")
{
    REQUIRE(desugar("fn f() { while x { y } }") == "fn f() { loop { if x { y } else break () } }");
    REQUIRE(
        desugar("fn f() { while let x = y { z } }")
        == "fn f() { loop { match y { immut x -> { z } _ -> break () } } }");
}

TEST("conditional expression")
{
    REQUIRE(desugar("fn f() { if x { y } }") == "fn f() { if x { y } else () }");
    REQUIRE(desugar("fn f() { if x { y } else { z } }") == "fn f() { if x { y } else { z } }");
    REQUIRE(
        desugar("fn f() { if let x = y { z } }")
        == "fn f() { match y { immut x -> { z } _ -> () } }");
    REQUIRE(
        desugar("fn f() { if let a = b { c } else { d } }")
        == "fn f() { match b { immut a -> { c } _ -> { d } } }");
}

TEST("discard expression")
{
    REQUIRE(desugar("fn f() { discard x; }") == "fn f() { { let _ = x; () }; () }");
}

TEST("struct")
{
    REQUIRE_SIMPLE_DESUGAR("struct S = a: Int, b: Float");
    REQUIRE_SIMPLE_DESUGAR("struct S[A, B] = a: A, b: B");
}

TEST("enum")
{
    REQUIRE_SIMPLE_DESUGAR("enum E = aaa | bbb(Int) | ccc(Float, Char)");
    REQUIRE_SIMPLE_DESUGAR("enum Option[T] = none | some(T)");
}

TEST("alias")
{
    REQUIRE_SIMPLE_DESUGAR("alias T = U");
    REQUIRE_SIMPLE_DESUGAR("alias A[B] = (B, B)");
}
