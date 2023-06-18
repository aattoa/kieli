#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    auto desugar(std::string&& string) -> std::string {
        (void)string;
        return {};
#if 0
        compiler::Compilation_info test_info = compiler::mock_compilation_info(utl::diagnostics::Level::suppress);
        utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));
        auto desugar_result = desugar(parse(kieli::lex({ .compilation_info = std::move(test_info), .source = test_source })));
        return std::format("{}", desugar_result.module.definitions);
#endif
    }
}

#define TEST(name) TEST_CASE(name, "[libdesugar]")


TEST("block expression") {
    REQUIRE(desugar("fn f() {}") == "fn f() = { () }");
    REQUIRE(desugar("fn f() { 5 }") == "fn f() = { 5 }");
    REQUIRE(desugar("fn f() { 5; }") == "fn f() = { 5; () }");
    REQUIRE(desugar("fn f() { 5; 10 }") == "fn f() = { 5; 10 }");
    REQUIRE(desugar("fn f() { 5; 10; }") == "fn f() = { 5; 10; () }");
}

TEST("function body normalization") {
    std::string_view const normalized = "fn f() = { 5 }";
    REQUIRE(normalized == desugar("fn f() { 5 }"));
    REQUIRE(normalized == desugar("fn f() = 5"));
    REQUIRE(normalized == desugar("fn f() = { 5 }"));
}

TEST("desugar while loop expression") {
    REQUIRE(desugar("fn f() { while x { y } }")
        == "fn f() = { loop { if x { y } else break () } }");
    REQUIRE(desugar("fn f() { while let x = y { z } }")
        == "fn f() = { loop { match y { x -> { z }_ -> break () } } }");
}

TEST("desugar conditional expression") {
    REQUIRE(desugar("fn f() { if x { y } }")
        == "fn f() = { if x { y } else () }");
    REQUIRE(desugar("fn f() { if x { y } else { z } }")
        == "fn f() = { if x { y } else { z } }");
    REQUIRE(desugar("fn f() { if let x = y { z } }")
        == "fn f() = { match y { x -> { z }_ -> () } }");
    REQUIRE(desugar("fn f() { if let a = b { c } else { d } }")
        == "fn f() = { match b { a -> { c }_ -> { d } } }");
}

TEST("desugar discard expression") {
    REQUIRE(desugar("fn f() { discard x; }")
        == "fn f() = { { let _ = x; () }; () }");
}
