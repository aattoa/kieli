#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "test_liblex.hpp"

#define TEST(name) TEST_CASE(name, "[liblex][quoted][literal]") // NOLINT

namespace {
    auto lex_success(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE(result.diagnostic_messages.empty());
        return result.formatted_tokens;
    }

    auto lex_failure(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE(result.formatted_tokens == "lexical error");
        return result.diagnostic_messages;
    }

    auto contains(std::string const& string)
    {
        return Catch::Matchers::ContainsSubstring(string, Catch::CaseSensitive::No);
    }
} // namespace

TEST("valid character literals")
{
    REQUIRE(
        lex_success("'x' 'y' '\\t' '\\\\'")
        == "(char: 'x'), (char: 'y'), (char: '\t'), (char: '\\')");
}

TEST("unterminating character literal")
{
    REQUIRE_THAT(lex_failure("'x"), contains("expected a closing single-quote"));
}

TEST("missing escape sequence")
{
    REQUIRE_THAT(lex_failure("'\\"), contains("expected an escape sequence"));
}

TEST("unrecognized escape sequence")
{
    REQUIRE_THAT(lex_failure("'\\w"), contains("unrecognized escape sequence"));
}

TEST("quote-character literal")
{
    REQUIRE(lex_success("''' '\"'") == "(char: '''), (char: '\"')");
}

TEST("valid string literals")
{
    REQUIRE(
        lex_success("\"test\t\\\",\", 'a', '\\\\'")
        == "(str: 'test\t\",'), ,, (char: 'a'), ,, (char: '\\')");
}

TEST("unterminating string literal")
{
    REQUIRE_THAT(lex_failure("\" wasd"), contains("unterminating string literal"));
}

TEST("comment within string literal")
{
    REQUIRE(lex_success("\" /* /* */ */ // \"") == "(str: ' /* /* */ */ // ')");
}

TEST("adjacent string literals")
{
    REQUIRE(lex_success("\"hello\" \"world\"") == "(str: 'hello'), (str: 'world')");
}
