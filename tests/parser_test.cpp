#include "utl/utilities.hpp"
#include "phase/parse/parser_internals.hpp"
#include <catch2/catch_test_macros.hpp>


namespace {
    template<auto extractor>
    auto make_node(std::string&& node_string) -> std::string {
        compiler::Compilation_info test_info = compiler::mock_compilation_info();
        utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(node_string));
        Parse_context parse_context {
            compiler::lex(compiler::Lex_arguments { .compilation_info = std::move(test_info), .source = test_source }),
            ast::Node_arena {}
        };
        return fmt::format("{}", extractor(parse_context));
    }

    constexpr auto expression = make_node<extract_expression>;
    constexpr auto pattern = make_node<extract_pattern>;
    constexpr auto type = make_node<extract_type>;
}


#define TEST(name) TEST_CASE(name, "[parse]")
#define REQUIRE_SIMPLE_PARSE(kind, string) REQUIRE(kind(string) == string)
#define REQUIRE_PARSE_FAILURE(...) REQUIRE_THROWS_AS((void)(__VA_ARGS__), utl::diagnostics::Error)


TEST("literal") {
    REQUIRE(expression("50") == "50");
    REQUIRE(expression("4.2e3") == "4200");
    REQUIRE(expression("\"Hello, \"     \"world!\"") == "\"Hello, world!\"");
}

TEST("block expression") {
    REQUIRE_SIMPLE_PARSE(expression, "{ }");
    REQUIRE_SIMPLE_PARSE(expression, "{ { } }");
    REQUIRE_SIMPLE_PARSE(expression, "{ { }; }");
    REQUIRE_SIMPLE_PARSE(expression, "{ { }; { } }");
}

TEST("conditional") {
    REQUIRE_SIMPLE_PARSE(expression, "if false { true; } else { 'a' }");
}

TEST("elif shorthand syntax") {
    REQUIRE(expression("if true { 50 } elif false { 75 } else { 100 }")
        == "if true { 50 } else if false { 75 } else { 100 }");
}

TEST("for loop") {
    REQUIRE_SIMPLE_PARSE(expression, "outer for x in \"hello\" { }");
}

TEST("operator precedence") {
    REQUIRE(expression("1 * 2 +$+ 3 + 4") == "((1 * 2) +$+ (3 + 4))");
}

TEST("duplicate initializer") {
    REQUIRE_PARSE_FAILURE(expression("S { a = ???, a = \"hello\" }"));
}

TEST("type cast") {
    REQUIRE(expression("'x' as U32 as Bool as Float + 3.14")
        == "(((('x' as U32) as Bool) as Float) + 3.14)");
}

TEST("member access") {
    REQUIRE_SIMPLE_PARSE(expression, "().1.2.[???].x.50.y.[0]");
}

TEST("method") {
    REQUIRE_SIMPLE_PARSE(expression, "x.y.f()");
}

TEST("let binding") {
    REQUIRE_SIMPLE_PARSE(expression, "let _: std::Vector[Long]::Element = 5");
}

TEST("implicit tuple let binding") {
    REQUIRE(expression("let a, mut b: (I64, Float) = (10, 20.5)")
        == "let (a, mut b): (I64, Float) = (10, 20.5)");
}

TEST("caseless match") {
    REQUIRE_PARSE_FAILURE(expression("match 0 {}"));
}

TEST("match") {
    REQUIRE_SIMPLE_PARSE(expression, "match x { 0 -> \"zero\" _ -> \"other\" }");
}

TEST("implicit tuple case") {
    REQUIRE(expression("match ??? { _, mut b, (c, _), [_] -> 1 }")
        == "match ??? { (_, mut b, (c, _), [_]) -> 1 }");
}

TEST("missing qualified name") {
    REQUIRE_PARSE_FAILURE(expression("::"));
    REQUIRE_PARSE_FAILURE(expression("test::"));
}

TEST("namespace access") {
    REQUIRE_SIMPLE_PARSE(expression, "::test");
}

TEST("template expression") {
    REQUIRE_SIMPLE_PARSE(expression, "std::Vector[Int, std::Allocator[Int]]::new()");
    REQUIRE_SIMPLE_PARSE(expression, "hello[]::nested[]::function[T]()");
}

TEST("tuple type") {
    REQUIRE_SIMPLE_PARSE(type, "()");
    REQUIRE_SIMPLE_PARSE(type, "(typeof(5), T)");
    REQUIRE(type("((()))") == "()");
    REQUIRE(type("(())") == "()");
}

TEST("type template instantiation") {
    REQUIRE_SIMPLE_PARSE(type, "Vec[Opt[typeof(sizeof(::Vec[Int]))]]");
}

TEST("tuple pattern") {
    REQUIRE_SIMPLE_PARSE(pattern, "()");
    REQUIRE_SIMPLE_PARSE(pattern, "(x, _)");
    REQUIRE(pattern("((()))") == "()");
    REQUIRE(pattern("(())") == "()");
}

TEST("enum constructor pattern") {
    REQUIRE_SIMPLE_PARSE(pattern, "Maybe::just(x)");
    REQUIRE_PARSE_FAILURE(pattern("Maybe::Just"));
}

TEST("as pattern") {
    REQUIRE_SIMPLE_PARSE(pattern, "(_, _) as mut x");
}
