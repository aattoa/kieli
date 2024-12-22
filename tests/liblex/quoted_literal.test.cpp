#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include "test_liblex.hpp"

#define TEST(name) UNITTEST("liblex quoted literal: " name)

namespace {
    auto lex_success(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE_EQUAL(result.diagnostic_messages, "");
        return result.formatted_tokens;
    }

    auto lex_failure(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE_EQUAL(result.formatted_tokens, "\"error\"");
        return result.diagnostic_messages;
    }
} // namespace

TEST("valid character literals")
{
    CHECK_EQUAL(
        lex_success("'x' 'y' '\\t' '\\\\'"),
        "(char: 'x'), (char: 'y'), (char: '\\t'), (char: '\\\\')");
}

TEST("unterminating character literal")
{
    CHECK(lex_failure("'x").contains("Expected a closing single-quote"));
}

TEST("missing escape sequence")
{
    CHECK(lex_failure("'\\").contains("Expected an escape sequence"));
}

TEST("unrecognized escape sequence")
{
    CHECK(lex_failure("'\\w").contains("Unrecognized escape sequence"));
}

TEST("quote-character literal")
{
    CHECK_EQUAL(lex_success("''' '\"'"), "(char: '\\''), (char: '\"')");
}

TEST("valid string literals")
{
    CHECK_EQUAL(
        lex_success("\"test\t\\\",\", 'a', '\\\\'"),
        R"((str: "test\t\","), ",", (char: 'a'), ",", (char: '\\'))");
}

TEST("unterminating string literal")
{
    CHECK(lex_failure("\" wasd").contains("Unterminating string literal"));
}

TEST("comment within string literal")
{
    CHECK_EQUAL(lex_success("\" /* /* */ */ // \""), "(str: \" /* /* */ */ // \")");
}

TEST("adjacent string literals")
{
    CHECK_EQUAL(lex_success("\"hello\" \"world\""), "(str: \"hello\"), (str: \"world\")");
}
