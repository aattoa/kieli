#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include "test_liblex.hpp"

#define TEST(name) UNITTEST("liblex floating literal: " name)

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

TEST("floating point literal explicit base rejection")
{
    CHECK(lex_failure("0x0.0").contains("A floating point literal may not have a base specifier"));
}

TEST("basic floating point syntax")
{
    CHECK_EQUAL(lex_success("3.14"), "(float: 3.14)");
    CHECK_EQUAL(lex_success(".314"), "., (int: 314)");
    CHECK(lex_failure("314.").contains("Expected one or more digits after the decimal separator"));
}

TEST("precending dot")
{
    CHECK_EQUAL(lex_success(".3.14"), "., (int: 3), ., (int: 14)");
    CHECK_EQUAL(lex_success(".3 .14"), "., (int: 3), ., (int: 14)");
    CHECK_EQUAL(lex_success(". 3.14"), "., (float: 3.14)");
}

TEST("floating point literal suffix")
{
    // SECTION("erroneous suffix")
    {
        CHECK(
            lex_failure("5.0wasd").contains("Erroneous floating point literal alphabetic suffix"));
    }
    // SECTION("valid suffix but missing exponent")
    {
        CHECK(lex_failure("5.0e").contains("Expected an exponent"));
        CHECK(lex_failure("5.0e-").contains("Expected an exponent"));
    }
    // SECTION("erroneous suffix after exponent")
    {
        CHECK(lex_failure("5.0e3wasd")
                  .contains("Erroneous floating point literal alphabetic suffix"));
    }
}

TEST("floating point literal exponent")
{
    // SECTION("positive exponent")
    {
        CHECK_EQUAL(lex_success("3.14e0"), "(float: 3.14)");
        CHECK_EQUAL(lex_success("3.14e1"), "(float: 31.4)");
        CHECK_EQUAL(lex_success("3.14e2"), "(float: 314)");
    }
    // SECTION("negative exponent")
    {
        CHECK_EQUAL(lex_success("3.14e-0"), "(float: 3.14)");
        CHECK_EQUAL(lex_success("3.14e-1"), "(float: 0.314)");
        CHECK_EQUAL(lex_success("3.14e-2"), "(float: 0.0314)");
    }
}

TEST("floating point literal out of valid range")
{
    CHECK(lex_failure("3.0e999").contains("Floating point literal is too large"));
}

TEST("floating point literal digit separators")
{
    // SECTION("valid literal")
    {
        CHECK_EQUAL(lex_success("1'2.3'4"), "(float: 12.34)");
    }
    // SECTION("digit separator preceding decimal separator")
    {
        auto const result = liblex::test_lex("1'.3");
        CHECK(result.diagnostic_messages.contains(
            "Expected one or more digits after the digit separator"));
        CHECK_EQUAL(result.formatted_tokens, "error, ., (int: 3)");
    }
    // SECTION("digit separator trailing decimal separator")
    {
        CHECK_EQUAL(lex_success("1'0.'3"), "(float: 10.3)");
    }
}
