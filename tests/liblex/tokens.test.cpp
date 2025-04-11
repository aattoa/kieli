#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <cppunittest/unittest.hpp>

using namespace ki;

namespace {
    auto tokens(std::string_view const document) -> std::string
    {
        auto state  = lex::state(document);
        auto output = std::string {};

        for (;;) {
            auto token = lex::next(state);
            if (token.type == lex::Type::End_of_input) {
                return output;
            }

            auto type = lex::token_type_string(token.type);
            auto text = token.view.string(document);

            if (text == type) {
                std::format_to(std::back_inserter(output), "('{}')", type);
            }
            else {
                std::format_to(std::back_inserter(output), "({}: {:?})", type, text);
            }
        }
    }

    constexpr auto keyword_strings = std::to_array<std::string_view>({
        "let",    "mut",     "immut",    "if",    "else",   "elif",   "for",    "in",
        "while",  "loop",    "continue", "break", "match",  "ret",    "fn",     "enum",
        "struct", "concept", "impl",     "alias", "import", "export", "module", "sizeof",
        "typeof", "where",   "dyn",      "macro", "global", "defer",
    });

    constexpr auto punctuation_strings = std::to_array<std::string_view>({
        ".", ",", ":",  ";",  "::", "&", "*", "+", "?", "!",
        "=", "|", "<-", "->", "(",  ")", "{", "}", "[", "]",
    });
} // namespace

UNITTEST("keyword tokens")
{
    for (auto const keyword : keyword_strings) {
        REQUIRE_EQUAL(tokens(keyword), std::format("('{}')", keyword));
    }
}

UNITTEST("punctuation tokens")
{
    for (auto const punctuation : punctuation_strings) {
        REQUIRE_EQUAL(tokens(punctuation), std::format("('{}')", punctuation));
    }
}

UNITTEST("identifier tokens")
{
    REQUIRE_EQUAL(tokens("hello world"), R"((lower: "hello")(lower: "world"))");
    REQUIRE_EQUAL(tokens("std::Vector"), R"((lower: "std")('::')(upper: "Vector"))");
}

UNITTEST("floating point tokens")
{
    REQUIRE_EQUAL(
        tokens("10. 1.1 2.2e2 0x0.0"),
        R"((float: "10.")(float: "1.1")(float: "2.2e2")(float: "0x0.0"))");
}

UNITTEST("integer tokens")
{
    REQUIRE_EQUAL(
        tokens(".1 2e2 0x0 0hello 10"),
        R"(('.')(int: "1")(int: "2e2")(int: "0x0")(int: "0hello")(int: "10"))");
}

UNITTEST("tuple field access")
{
    REQUIRE_EQUAL(tokens("x.0"), R"((lower: "x")('.')(int: "0"))");
    REQUIRE_EQUAL(tokens("x.0.0"), R"((lower: "x")('.')(int: "0")('.')(int: "0"))");
}

UNITTEST("available operators")
{
    REQUIRE_EQUAL(
        tokens("-- %?% <$> ** @#"), R"((op: "--")(op: "%?%")(op: "<$>")(op: "**")(op: "@#"))");
}
