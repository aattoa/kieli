#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "test_liblex.hpp"

#define TEST(name) TEST_CASE(name, "[liblex][keyword][identifier]") // NOLINT

namespace {
    auto lex_success(std::string&& string) -> std::string {
        auto result = liblex::test_lex(std::move(string));
        REQUIRE(result.diagnostic_messages.empty());
        return result.formatted_tokens;
    }
    constexpr auto keywords = std::to_array<std::string_view>({
        "let", "mut", "immut", "if", "else", "elif", "for", "in", "while", "loop",
        "continue", "break", "match", "ret", "discard", "fn", "as", "enum", "struct",
        "class", "inst", "impl", "alias", "namespace", "import", "export", "module",
        "sizeof", "typeof", "addressof", "dereference", "unsafe", "mov", "meta", "where",
        "dyn", "pub", "macro", "global", "String", "Float", "Char", "Bool", "I8", "I16",
        "I32", "I64", "U8", "U16", "U32", "U64", "self", "Self",
    });
}


TEST("keywords") {
    for (auto const keyword : keywords) {
        REQUIRE(lex_success(std::string(keyword)) == fmt::format("{}, end of input", keyword));
    }
}

TEST("boolean literals") {
    REQUIRE(lex_success("true") == "(bool: 'true'), end of input");
    REQUIRE(lex_success("false") == "(bool: 'false'), end of input");
}

TEST("underscores") {
    REQUIRE(lex_success("_")     == "_, end of input");
    REQUIRE(lex_success("_____") == "_, end of input");
}

TEST("uncapitalized identifiers") {
    REQUIRE(lex_success("a bBb for_ forR _x ___x___ _5") ==
        "(lower: 'a'), (lower: 'bBb'), (lower: 'for_'), "
        "(lower: 'forR'), (lower: '_x'), (lower: '___x___'), (lower: '_5'), end of input");
}

TEST("capitalized identifiers") {
    REQUIRE(lex_success("A Bbb For_ FORR _X ___X___") ==
        "(upper: 'A'), (upper: 'Bbb'), (upper: 'For_'), "
        "(upper: 'FORR'), (upper: '_X'), (upper: '___X___'), end of input");
}