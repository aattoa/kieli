#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    auto desugar(std::string&& string) -> std::string {
        compiler::Compilation_info test_info = compiler::mock_compilation_info(utl::diagnostics::Level::suppress);
        utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));
        auto desugar_result = desugar(parse(kieli::lex({ .compilation_info = std::move(test_info), .source = test_source })));
        std::string output;
        for (ast::Definition const& definition : desugar_result.module.definitions)
            ast::format_to(definition, output);
        return output;
    }
}

#define TEST(name) TEST_CASE("desugar " name, "[libdesugar]") // NOLINT


// TODO: struct initializer expressions


TEST("block expression") {
    REQUIRE(desugar("fn f() {}") == "fn f() { () }");
    REQUIRE(desugar("fn f() { 5 }") == "fn f() { 5 }");
    REQUIRE(desugar("fn f() { 5; }") == "fn f() { 5; () }");
    REQUIRE(desugar("fn f() { 5; 10 }") == "fn f() { 5; 10 }");
    REQUIRE(desugar("fn f() { 5; 10; }") == "fn f() { 5; 10; () }");
}

TEST("function body normalization") {
    static constexpr std::string_view normalized = "fn f() { 5 }";
    REQUIRE(normalized == desugar("fn f() { 5 }"));
    REQUIRE(normalized == desugar("fn f() = 5"));
    REQUIRE(normalized == desugar("fn f() = { 5 }"));
}

TEST("operator precedence") {
    /*
        precedence table:
        "*", "/", "%"
        "+", "-"
        "?=", "!="
        "<", "<=", ">=", ">"
        "&&", "||"
        ":=", "+=", "*=", "/=", "%="
     */
    REQUIRE(desugar("fn f() { (a * b + c) + (d + e * f) }")
        == "fn f() { (((a * b) + c) + (d + (e * f))) }");
    REQUIRE(desugar("fn f() { a <$> b && c <= d ?= e + f / g }")
        == "fn f() { (a <$> (b && (c <= (d ?= (e + (f / g)))))) }");
    REQUIRE(desugar("fn f() { a / b + c ?= d <= e && f <$> g }")
        == "fn f() { ((((((a / b) + c) ?= d) <= e) && f) <$> g) }");
    REQUIRE(desugar("fn f() { a + b && c }")
        == "fn f() { ((a + b) && c) }");
    REQUIRE(desugar("fn f() { a %% c % d ?= e }")
        == "fn f() { (a %% ((c % d) ?= e)) }");
}

TEST("desugar while loop expression") {
    REQUIRE(desugar("fn f() { while x { y } }")
        == "fn f() { loop { if x { y } else break () } }");
    REQUIRE(desugar("fn f() { while let x = y { z } }")
        == "fn f() { loop { match y { immut x -> { z } _ -> break () } } }");
}

TEST("desugar conditional expression") {
    REQUIRE(desugar("fn f() { if x { y } }")
        == "fn f() { if x { y } else () }");
    REQUIRE(desugar("fn f() { if x { y } else { z } }")
        == "fn f() { if x { y } else { z } }");
    REQUIRE(desugar("fn f() { if let x = y { z } }")
        == "fn f() { match y { immut x -> { z } _ -> () } }");
    REQUIRE(desugar("fn f() { if let a = b { c } else { d } }")
        == "fn f() { match b { immut a -> { c } _ -> { d } } }");
}

TEST("desugar discard expression") {
    REQUIRE(desugar("fn f() { discard x; }")
        == "fn f() { { let _ = x; () }; () }");
}
