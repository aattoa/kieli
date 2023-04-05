#include "utl/utilities.hpp"
#include "lex.hpp"
#include "tests/tests.hpp"


namespace {

    constinit thread_local compiler::Program_string_pool* test_string_pool_ptr = nullptr;

    using Token = compiler::Lexical_token;

    auto assert_tok_eq(
        std::string_view     const text,
        std::vector<Token::Type>   required_types,
        std::source_location const caller = std::source_location::current()) -> void
    {
        utl::always_assert(test_string_pool_ptr != nullptr);

        utl::Source source { utl::Source::Filename { "[TEST]" }, std::string { text } };
        auto lex_result = compiler::lex({ std::move(source), *test_string_pool_ptr });

        required_types.push_back(Token::Type::end_of_input);

        auto const actual_types = lex_result.tokens
            | ranges::views::transform(&Token::type)
            | ranges::to<std::vector>();

        tests::assert_eq(required_types, actual_types, caller);
    }


    auto run_lexer_tests() -> void {
        using namespace tests;
        using enum Token::Type;

        compiler::Program_string_pool string_pool;
        test_string_pool_ptr = &string_pool;

        "whitespace"_test = [] {
            assert_tok_eq(
                "\ta\nb  \t  c  \n  d\n\n e ",
                { lower_name
                , lower_name
                , lower_name
                , lower_name
                , lower_name });
        };

        "numeric"_test = [] {
            assert_tok_eq(
                "23.4 1.",
                { floating, floating });

            assert_tok_eq(
                "50 0xdeadbeef -3 3e3 18446744073709551615",
                { integer_of_unknown_sign
                , integer_of_unknown_sign
                , signed_integer
                , integer_of_unknown_sign
                , unsigned_integer });

            assert_tok_eq(
                "0.3e-5 -0. -0.2E5",
                { floating, floating, floating });
        };

        "tuple_member_access"_test = [] {
            assert_tok_eq(
                ".0.0, 0.0",
                { dot
                , integer_of_unknown_sign
                , dot
                , integer_of_unknown_sign
                , comma
                , floating });
        };

        "punctuation"_test = [] {
            assert_tok_eq(
                "\n::\t,;(--? @#",
                { double_colon
                , comma
                , semicolon
                , paren_open
                , operator_name
                , operator_name });
        };

        "comment"_test = [] {
            assert_tok_eq(
                ". /* , /*::*/! */ in /**/ / //",
                { dot, in, operator_name });

            assert_tok_eq(
                R"(/* "" */ . /* "*/" */ . "/* /*" . /* /* "*/"*/ */ .)",
                { dot, dot, string, dot, dot });
        };

        "keyword"_test = [] {
            assert_tok_eq(
                "for;forr(for2",
                { for_, semicolon, lower_name, paren_open, lower_name });

            assert_tok_eq(
                ",.[}\tmatch::",
                { comma
                , dot
                , bracket_open
                , brace_close
                , match
                , double_colon });
        };

        "pattern"_test = [] {
            assert_tok_eq(
                "x1 _ wasd,3",
                { lower_name
                , underscore
                , lower_name
                , comma
                , integer_of_unknown_sign });

            assert_tok_eq(
                "a<$>_:\nVec",
                { lower_name, operator_name, underscore, colon, upper_name });

            assert_tok_eq(
                "_, ______::_________________",
                { underscore, comma, underscore, double_colon, underscore });
        };

        "string"_test = [] {
            assert_tok_eq(
                "\"test\\t\\\",\", 'a', '\\\\'", // NOLINT
                { string, comma, character, comma, character });

            assert_tok_eq(
                R"("hmm" ", yes")",
                { string });
        };

        "casing"_test = [] {
            assert_tok_eq(
                "a A _a _A _0 _",
                { lower_name
                , upper_name
                , lower_name
                , upper_name
                , lower_name
                , underscore });
        };
    }

}


REGISTER_TEST(run_lexer_tests);
