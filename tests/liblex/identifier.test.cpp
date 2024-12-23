#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include "test_liblex.hpp"

#define TEST(name) UNITTEST("liblex identifiers: " name)

namespace {
    auto lex_success(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE_EQUAL(result.diagnostic_messages, "");
        return result.formatted_tokens;
    }

    constexpr auto keywords = std::to_array<std::string_view>({
        "let",    "mut",     "immut",    "if",    "else",   "elif",   "for",    "in",
        "while",  "loop",    "continue", "break", "match",  "ret",    "fn",     "enum",
        "struct", "concept", "impl",     "alias", "import", "export", "module", "sizeof",
        "typeof", "mv",      "where",    "dyn",   "macro",  "global", "defer",
    });
} // namespace

TEST("keywords")
{
    for (auto const keyword : keywords) {
        CHECK_EQUAL(lex_success(std::string(keyword)), std::format("\"{}\"", keyword));
    }
}

TEST("boolean literals")
{
    CHECK_EQUAL(lex_success("true"), "(bool: true)");
    CHECK_EQUAL(lex_success("false"), "(bool: false)");
}

TEST("underscores")
{
    CHECK_EQUAL(lex_success("_"), R"("_")");
    CHECK_EQUAL(lex_success("_____"), R"("_")");
}

TEST("uncapitalized identifiers")
{
    CHECK_EQUAL(
        lex_success("a bBb for_ forR _x ___x___ _5"),
        "(lower: a), (lower: bBb), (lower: for_), "
        "(lower: forR), (lower: _x), (lower: ___x___), (lower: _5)");
}

TEST("capitalized identifiers")
{
    CHECK_EQUAL(
        lex_success("A Bbb For_ FORR _X ___X___"),
        "(upper: A), (upper: Bbb), (upper: For_), "
        "(upper: FORR), (upper: _X), (upper: ___X___)");
}
