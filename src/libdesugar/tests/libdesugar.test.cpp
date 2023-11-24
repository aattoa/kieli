#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    auto desugar(std::string&& string) -> std::string
    {
        auto [info, source] = kieli::test_info_and_source(std::move(string));
        auto const tokens   = kieli::lex(source, info);
        auto const module   = desugar(parse(tokens, info), info);

        std::string output;
        for (ast::Definition const& definition : module.definitions) {
            ast::format_to(definition, output);
        }
        return output;
    }
} // namespace

#define TEST(name) TEST_CASE("desugar " name, "[libdesugar]") // NOLINT

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
