#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <cppunittest/unittest.hpp>

namespace {
    auto lex(std::string&& string) -> std::string
    {
        auto [info, source] = kieli::test_info_and_source(std::move(string));
        auto state          = kieli::Lex_state::make(source, info);

        std::string output;
        for (;;) {
            auto token = kieli::lex(state);
            std::format_to(
                std::back_inserter(output), "('{}' {})", token.preceding_trivia, token.type);
            if (token.type == kieli::Token::Type::end_of_input) {
                return output;
            }
        }
    }
} // namespace

#define TEST(name) UNITTEST("liblex trivia: " name)

TEST("whitespace trivia")
{
    CHECK_EQUAL(
        lex("\ta\nb  \t  c  \n  d\n\n e "),
        "('\t' lower)"
        "('\n' lower)"
        "('  \t  ' lower)"
        "('  \n  ' lower)"
        "('\n\n ' lower)"
        "(' ' end of input)");

    CHECK_EQUAL(lex(" \t \n "), "(' \t \n ' end of input)");
}

TEST("line comment trivia")
{
    CHECK_EQUAL(
        lex(" a // b \n c // d"),
        "(' ' lower)"
        "(' // b \n ' lower)"
        "(' // d' end of input)");
}

TEST("block commen trivia")
{
    CHECK_EQUAL(
        lex(". /* , /*::*/! */ in /**/ / //"),
        "('' .)"
        "(' /* , /*::*/! */ ' in)"
        "(' /**/ ' op)"
        "(' //' end of input)");

    CHECK_EQUAL(
        lex(R"(/* "" */ . /* "*/" */ . "/* /*" . /* /* "*/"*/ */ .)"),
        "('/* \"\" */ ' .)"
        "(' /* \"*/\" */ ' .)"
        "(' ' str)"
        "(' ' .)"
        "(' /* /* \"*/\"*/ */ ' .)"
        "('' end of input)");
}
