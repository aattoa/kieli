#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include "test_liblex.hpp"

#define TEST(name) UNITTEST("liblex integer literal: " name)

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
        REQUIRE_EQUAL(result.formatted_tokens, "error");
        return result.diagnostic_messages;
    }
} // namespace

TEST("integer literal bases")
{
    // SECTION("binary")
    {
        CHECK_EQUAL(lex_success("0b100"), "(int: 4)");
        CHECK_EQUAL(lex_success("0b100e2"), "(int: 400)");
    }
    // SECTION("quaternary")
    {
        CHECK_EQUAL(lex_success("0q100"), "(int: 16)");
        CHECK_EQUAL(lex_success("0q100e2"), "(int: 1600)");
    }
    // SECTION("octal")
    {
        CHECK_EQUAL(lex_success("0o100"), "(int: 64)");
        CHECK_EQUAL(lex_success("0o100e2"), "(int: 6400)");
    }
    // SECTION("decimal")
    {
        CHECK_EQUAL(lex_success("100"), "(int: 100)");
        CHECK_EQUAL(lex_success("100e2"), "(int: 10000)");
    }
    // SECTION("duodecimal")
    {
        CHECK_EQUAL(lex_success("0d100"), "(int: 144)");
        CHECK_EQUAL(lex_success("0d100e2"), "(int: 14400)");
    }
    // SECTION("hexadecimal")
    {
        CHECK_EQUAL(lex_success("0x100"), "(int: 256)");
        CHECK_EQUAL(lex_success("0xdeadbeef"), "(int: 3735928559)");
        // TODO: figure out how to handle hex exponents
    }
}

TEST("integer literal suffix")
{
    // SECTION("erroneous suffix")
    {
        CHECK(lex_failure("5wasd").contains("Erroneous integer literal alphabetic suffix"));
    }
    // SECTION("valid suffix but missing exponent")
    {
        CHECK(lex_failure("5e").contains("Expected an exponent"));
    }
    // SECTION("valid suffix but negative exponent")
    {
        CHECK(lex_failure("5e-3").contains("negative exponent"));
    }
    // SECTION("valid exponent")
    {
        CHECK_EQUAL(lex_success("5e3"), "(int: 5000)");
    }
    // SECTION("erroneous suffix after exponent")
    {
        CHECK(lex_failure("5e3wasd").contains("Erroneous integer literal alphabetic suffix"));
    }
}

TEST("integer literal valid range")
{
    CHECK_EQUAL(
        lex_success(std::format("{}", std::numeric_limits<std::size_t>::max())),
        std::format("(int: {})", std::numeric_limits<std::size_t>::max()));
    CHECK(lex_failure("18446744073709551616").contains("Integer literal is too large"));
    CHECK(lex_failure("5e18446744073709551616").contains("Exponent is too large"));
    CHECK(lex_failure("5e20").contains("too large after applying scientific exponent"));
}

TEST("integer literal digit separators")
{
    CHECK_EQUAL(lex_success("123'456'789"), "(int: 123456789)");
    CHECK_EQUAL(lex_success("1'2'3'4'5'6'7'8'9"), "(int: 123456789)");
    CHECK_EQUAL(lex_success("0x123'abc"), "(int: 1194684)");
    CHECK_EQUAL(lex_success("0x'123'abc"), "(int: 1194684)");
    CHECK(lex_failure("1'").contains("Expected one or more digits after the digit separator"));
    CHECK(lex_failure("0x'").contains("Expected one or more digits after the base-16 specifier"));
}
