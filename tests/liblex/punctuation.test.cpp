#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include "test_liblex.hpp"

#define TEST(name) UNITTEST("liblex: punctuation and operators: " name)

namespace {
    auto lex_success(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE_EQUAL(result.diagnostic_messages, "");
        return result.formatted_tokens;
    }

    constexpr auto punctuation_strings = std::to_array<std::string_view>({
        ".", ",", ":",  ";",  "::", "&", "*", "+", "?", "!",
        "=", "|", "<-", "->", "(",  ")", "{", "}", "[", "]",
    });
} // namespace

TEST("punctuation and reserved operators")
{
    for (auto const punct : punctuation_strings) {
        CHECK_EQUAL(lex_success(std::string(punct)), std::format("\"{}\"", punct));
    }
}

TEST("available operators")
{
    CHECK_EQUAL(
        lex_success("-- %?% <$> ** @#"), "(op: --), (op: %?%), (op: <$>), (op: **), (op: @#)");
}

TEST("operators and punctuation tokens mixed")
{
    CHECK_EQUAL(lex_success("\n::\t,;(--?}@@"), R"("::", ",", ";", "(", (op: --?), "}", (op: @@))");
}
