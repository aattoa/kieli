#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "test_liblex.hpp"

#define TEST(name) TEST_CASE(name, "[liblex][floating][literal]") // NOLINT

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

TEST("floating point literal explicit base rejection")
{
    REQUIRE_THAT(
        lex_failure("0x0.0"), contains("a floating point literal may not have a base specifier"));
}

TEST("basic floating point syntax")
{
    REQUIRE(lex_success("3.14") == "(float: 3.14)");
    REQUIRE(lex_success(".314") == "., (int: 314)");
    REQUIRE_THAT(
        lex_failure("314."), contains("expected one or more digits after the decimal separator"));
}

TEST("precending dot")
{
    REQUIRE(lex_success(".3.14") == "., (int: 3), ., (int: 14)");
    REQUIRE(lex_success(".3 .14") == "., (int: 3), ., (int: 14)");
    REQUIRE(lex_success(". 3.14") == "., (float: 3.14)");
}

TEST("floating point literal suffix")
{
    SECTION("erroneous suffix")
    {
        REQUIRE_THAT(
            lex_failure("5.0wasd"), contains("erroneous floating point literal alphabetic suffix"));
    }
    SECTION("valid suffix but missing exponent")
    {
        REQUIRE_THAT(lex_failure("5.0e"), contains("expected an exponent"));
        REQUIRE_THAT(lex_failure("5.0e-"), contains("expected an exponent"));
    }
    SECTION("erroneous suffix after exponent")
    {
        REQUIRE_THAT(
            lex_failure("5.0e3wasd"),
            contains("erroneous floating point literal alphabetic suffix"));
    }
}

TEST("floating point literal exponent")
{
    SECTION("positive exponent")
    {
        REQUIRE(lex_success("3.14e0") == "(float: 3.14)");
        REQUIRE(lex_success("3.14e1") == "(float: 31.4)");
        REQUIRE(lex_success("3.14e2") == "(float: 314)");
    }
    SECTION("negative exponent")
    {
        REQUIRE(lex_success("3.14e-0") == "(float: 3.14)");
        REQUIRE(lex_success("3.14e-1") == "(float: 0.314)");
        REQUIRE(lex_success("3.14e-2") == "(float: 0.0314)");
    }
}

TEST("floating point literal out of valid range")
{
    REQUIRE_THAT(lex_failure("3.0e999"), contains("floating point literal is too large"));
}

TEST("floating point literal digit separators")
{
    SECTION("valid literal")
    {
        REQUIRE(lex_success("1'2.3'4") == "(float: 12.34)");
    }
    SECTION("digit separator preceding decimal separator")
    {
        auto const result = liblex::test_lex("1'.3");
        REQUIRE_THAT(
            result.diagnostic_messages,
            contains("expected one or more digits after the digit separator"));
        REQUIRE(result.formatted_tokens == "lexical error, ., (int: 3)");
    }
    SECTION("digit separator trailing decimal separator")
    {
        REQUIRE(lex_success("1'0.'3") == "(float: 10.3)");
    }
}
