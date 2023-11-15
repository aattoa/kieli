#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "test_liblex.hpp"

#define TEST(name) TEST_CASE(name, "[liblex][integer][literal]") // NOLINT

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
        REQUIRE(result.formatted_tokens == "lexical error, end of input");
        return result.diagnostic_messages;
    }

    auto contains(std::string const& string)
    {
        return Catch::Matchers::ContainsSubstring(string, Catch::CaseSensitive::No);
    }
} // namespace

TEST("integer literal bases")
{
    SECTION("binary")
    {
        REQUIRE(lex_success("0b100") == "(int: 4), end of input");
        REQUIRE(lex_success("0b100e2") == "(int: 400), end of input");
    }
    SECTION("quaternary")
    {
        REQUIRE(lex_success("0q100") == "(int: 16), end of input");
        REQUIRE(lex_success("0q100e2") == "(int: 1600), end of input");
    }
    SECTION("octal")
    {
        REQUIRE(lex_success("0o100") == "(int: 64), end of input");
        REQUIRE(lex_success("0o100e2") == "(int: 6400), end of input");
    }
    SECTION("decimal")
    {
        REQUIRE(lex_success("100") == "(int: 100), end of input");
        REQUIRE(lex_success("100e2") == "(int: 10000), end of input");
    }
    SECTION("duodecimal")
    {
        REQUIRE(lex_success("0d100") == "(int: 144), end of input");
        REQUIRE(lex_success("0d100e2") == "(int: 14400), end of input");
    }
    SECTION("hexadecimal")
    {
        REQUIRE(lex_success("0x100") == "(int: 256), end of input");
        REQUIRE(lex_success("0xdeadbeef") == "(int: 3735928559), end of input");
        // TODO: figure out how to handle hex exponents
    }
}

TEST("integer literal suffix")
{
    SECTION("erroneous suffix")
    {
        REQUIRE_THAT(lex_failure("5wasd"), contains("erroneous integer literal alphabetic suffix"));
    }
    SECTION("valid suffix but missing exponent")
    {
        REQUIRE_THAT(lex_failure("5e"), contains("expected an exponent"));
    }
    SECTION("valid suffix but negative exponent")
    {
        REQUIRE_THAT(lex_failure("5e-3"), contains("negative exponent"));
    }
    SECTION("valid exponent")
    {
        REQUIRE(lex_success("5e3") == "(int: 5000), end of input");
    }
    SECTION("erroneous suffix after exponent")
    {
        REQUIRE_THAT(
            lex_failure("5e3wasd"), contains("erroneous integer literal alphabetic suffix"));
    }
}

TEST("integer literal valid range")
{
    REQUIRE(
        lex_success(std::format("{}", std::numeric_limits<utl::Usize>::max()))
        == std::format("(int: {}), end of input", std::numeric_limits<utl::Usize>::max()));
    REQUIRE_THAT(lex_failure("18446744073709551616"), contains("integer literal is too large"));
    REQUIRE_THAT(lex_failure("5e18446744073709551616"), contains("exponent is too large"));
    REQUIRE_THAT(lex_failure("5e20"), contains("too large after applying scientific exponent"));
}

TEST("integer literal digit separators")
{
    REQUIRE(lex_success("123'456'789") == "(int: 123456789), end of input");
    REQUIRE(lex_success("1'2'3'4'5'6'7'8'9") == "(int: 123456789), end of input");
    REQUIRE(lex_success("0x123'abc") == "(int: 1194684), end of input");
    REQUIRE(lex_success("0x'123'abc") == "(int: 1194684), end of input");
    REQUIRE_THAT(
        lex_failure("1'"), contains("expected one or more digits after the digit separator"));
    REQUIRE_THAT(
        lex_failure("0x'"), contains("expected one or more digits after the base specifier"));
}
