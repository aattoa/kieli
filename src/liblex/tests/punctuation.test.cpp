#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "test_liblex.hpp"

#define TEST(name) TEST_CASE(name, "[liblex][punctuation][operator]") // NOLINT

namespace {
    auto lex_success(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE(result.diagnostic_messages == "");
        return result.formatted_tokens;
    }

    constexpr auto punctuation_strings = std::to_array<std::string_view>({
        ".",  ",",  ":",  ";",      "::", "&", "*", "+", "?", "=", "|",
        "\\", "<-", "->", R"(???)", "(",  ")", "{", "}", "[", "]",
    });
} // namespace

TEST("punctuation and reserved operators")
{
    for (auto const punctuation_string : punctuation_strings) {
        REQUIRE(lex_success(std::string(punctuation_string)) == punctuation_string);
    }
}

TEST("available operators")
{
    REQUIRE(
        lex_success("-- %?% <$> ** @#") == "(op: --), (op: %?%), (op: <$>), (op: **), (op: @#)");
}

TEST("operators and punctuation tokens mixed")
{
    REQUIRE(lex_success("\n::\t,;(--?}@@") == "::, ,, ;, (, (op: --?), }, (op: @@)");
}
