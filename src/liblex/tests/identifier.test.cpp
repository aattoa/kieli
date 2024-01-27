#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "test_liblex.hpp"

#define TEST(name) TEST_CASE(name, "[liblex][keyword][identifier]") // NOLINT

namespace {
    auto lex_success(std::string&& string) -> std::string
    {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE(result.diagnostic_messages == "");
        return result.formatted_tokens;
    }

    constexpr auto keywords = std::to_array<std::string_view>({
        "let",    "mut",    "immut",    "if",     "else",   "elif",      "for",         "in",
        "while",  "loop",   "continue", "break",  "match",  "ret",       "discard",     "fn",
        "as",     "enum",   "struct",   "class",  "inst",   "impl",      "alias",       "namespace",
        "import", "export", "module",   "sizeof", "typeof", "addressof", "dereference", "unsafe",
        "mov",    "meta",   "where",    "dyn",    "macro",  "global",    "String",      "Float",
        "Char",   "Bool",   "I8",       "I16",    "I32",    "I64",       "U8",          "U16",
        "U32",    "U64",    "self",     "Self",
    });
} // namespace

TEST("keywords")
{
    for (auto const keyword : keywords) {
        REQUIRE(lex_success(std::string(keyword)) == std::format("{}", keyword));
    }
}

TEST("boolean literals")
{
    REQUIRE(lex_success("true") == "(bool: true)");
    REQUIRE(lex_success("false") == "(bool: false)");
}

TEST("underscores")
{
    REQUIRE(lex_success("_") == "_");
    REQUIRE(lex_success("_____") == "_");
}

TEST("uncapitalized identifiers")
{
    REQUIRE(
        lex_success("a bBb for_ forR _x ___x___ _5")
        == "(lower: a), (lower: bBb), (lower: for_), "
           "(lower: forR), (lower: _x), (lower: ___x___), (lower: _5)");
}

TEST("capitalized identifiers")
{
    REQUIRE(
        lex_success("A Bbb For_ FORR _X ___X___")
        == "(upper: A), (upper: Bbb), (upper: For_), "
           "(upper: FORR), (upper: _X), (upper: ___X___)");
}
