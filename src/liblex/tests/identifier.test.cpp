#include <libutl/common/utilities.hpp>
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
        "let",    "mut",    "immut",    "if",     "else",      "elif",        "for",     "in",
        "while",  "loop",   "continue", "break",  "match",     "ret",         "discard", "fn",
        "as",     "enum",   "struct",   "class",  "inst",      "impl",        "alias",   "import",
        "export", "module", "sizeof",   "typeof", "addressof", "dereference", "unsafe",  "mov",
        "meta",   "where",  "dyn",      "macro",  "global",    "String",      "Float",   "Char",
        "Bool",   "I8",     "I16",      "I32",    "I64",       "U8",          "U16",     "U32",
        "U64",    "self",   "Self",
    });
} // namespace

TEST("keywords")
{
    for (auto const keyword : keywords) {
        CHECK_EQUAL(lex_success(std::string(keyword)), keyword);
    }
}

TEST("boolean literals")
{
    CHECK_EQUAL(lex_success("true"), "(bool: true)");
    CHECK_EQUAL(lex_success("false"), "(bool: false)");
}

TEST("underscores")
{
    CHECK_EQUAL(lex_success("_"), "_");
    CHECK_EQUAL(lex_success("_____"), "_");
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
