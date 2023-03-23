#include "utl/utilities.hpp"
#include "tests/tests.hpp"

#include "parser/parser_internals.hpp"


namespace {

    constinit compiler::Program_string_pool* test_string_pool = nullptr;


    template <auto extractor>
    auto node_test(
        std::string                     node_string,
        tl::optional<std::string> const expected_string = tl::nullopt,
        std::source_location      const caller = std::source_location::current()) -> void
    {
        utl::always_assert(test_string_pool != nullptr);

        auto parse_context = std::invoke([&] {
            utl::Source source {
                utl::Source::Mock_tag { .filename = "TEST" },
                utl::copy(node_string)
            };
            utl::diagnostics::Builder::Configuration const configuration {
                .note_level = utl::diagnostics::Level::suppress,
                .warning_level = utl::diagnostics::Level::suppress
            };
            return Parse_context {
                compiler::lex(std::move(source), *test_string_pool, configuration)
            };
        });

        auto const extracted_node = extractor(parse_context);
        std::string formatted_node = fmt::format("{}", extracted_node);

        static auto const remove_whitespace = [&](std::string copy) {
            copy.erase(std::remove_if(begin(copy), end(copy), [](char const c) { return c == ' ' || c == '\t' || c == '\n'; }), end(copy));
            return copy;
        };

        tests::assert_eq(
            remove_whitespace(formatted_node),
            remove_whitespace(expected_string.value_or(node_string)),
            caller
        );
    }

    auto expression(
        std::string                     expression_string,
        tl::optional<std::string> const expected_string = tl::nullopt,
        std::source_location      const caller = std::source_location::current()) -> void
    {
        node_test<extract_expression>(std::move(expression_string), expected_string, caller);
    }
    auto type(
        std::string                     type_string,
        tl::optional<std::string> const expected_string = tl::nullopt,
        std::source_location      const caller = std::source_location::current()) -> void
    {
        node_test<extract_type>(std::move(type_string), expected_string, caller);
    }
    auto pattern(
        std::string                      pattern_string,
        tl::optional<std::string> const expected_string = tl::nullopt,
        std::source_location       const caller = std::source_location::current()) -> void
    {
        node_test<extract_pattern>(std::move(pattern_string), expected_string, caller);
    }


    auto run_parser_tests() -> void {
        using namespace utl::literals;
        using namespace tests;

        ast::Node_context node_context;
        compiler::Program_string_pool string_pool;
        test_string_pool = &string_pool;


        "literal"_test = [] {
            expression("50");
            expression("4.2e3", "4200");
            expression("\"Hello, \"     \"world!\"", "\"Hello, world!\"");
        };

        "block"_test = [] {
            expression("{}");
            expression("{ {} }");
            expression("{ {}; }");
            expression("{ {}; {} }");
        };

        "conditional"_test = [] {
            expression("if false { true; } else { 'a' }");
        };

        "conditional_elif"_test = [] {
            expression(
                "if true { 50 } elif false { 75 } else { 100 }",
                "if true { 50 } else if false { 75 } else { 100 }"
            );
        };

        "for_loop"_test = [] {
            expression("outer for x in \"hello\" {}");
        };

        "operator_precedence"_test = [] {
            expression(
                "1 * 2 +$+ 3 + 4",
                "((1*2) +$+ (3+4))"
            );
        };

        "duplicate_initializer"_throwing_test = [] {
            expression("S { a = ???, a = \"hello\" }");
        };

        "type_cast"_test = [] {
            expression(
                "'x' as U32 as Bool as Float + 3.14",
                "(((('x' as U32) as Bool) as Float) + 3.14)"
            );
        };

        "member_access"_test = [] {
            expression(
                "().1.2.[???].x.50.y.[0]",
                "(().1.2.[???].x.50.y.[0])"
            );
        };

        "method"_test = [] {
            expression("x.y.f()", "(x.y).f()");
        };

        "let_binding"_test = [] {
            expression("let _: std::Vector[Long]::Element = 5");
        };

        "implicit_tuple_let_binding"_test = [] {
            expression(
                "let a, mut b: (I64, Float) = (10, 20.5)",
                "let (a, mut b): (I64, Float) = (10, 20.5)"
            );
        };

        "caseless_match"_throwing_test = [] {
            expression("match 0 {}");
        };

        "match"_test = [] {
            expression("match x { 0 -> \"zero\" _ -> \"other\" }");
        };

        "implicit_tuple_case_match"_test = [] {
            expression(
                "match ??? { _, mut b, (c, _), [_] -> 1 }",
                "match ??? { (_, mut b, (c, _), [_]) -> 1 }"
            );
        };

        "scope_access_1"_throwing_test = [] {
            expression("::");
        };

        "scope_access_2"_throwing_test = [] {
            expression("test::");
        };

        "scope_access_3"_test = [] {
            expression("::test");
        };

        "template_expression"_test = [] {
            expression("std::Vector[Int, std::Allocator[Int]]::new()");
            expression("hello[]::nested[]::function[T]()");
        };


        "tuple_type"_test = [] {
            type("()");
            type("(())", "()");
            type("(typeof_(5), T)");
        };

        "template_type"_test = [] {
            type("Vec[Opt[typeof_(sizeof(::Vec[Int]))]]");
        };



        "tuple_pattern"_test = [] {
            pattern("()");
            pattern("(())", "()");
            pattern("(x, _)");
        };

        "enum_constructor_pattern"_test = [] {
            pattern("Maybe::just(x)");
        };

        "enum_constructor_pattern"_throwing_test = [] {
            pattern("Maybe::Just");
        };

        "as_pattern"_test = [] {
            pattern("(_, _) as mut x");
        };

    }

}


REGISTER_TEST(run_parser_tests);