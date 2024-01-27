#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    auto lex(std::string&& string) -> std::string
    {
        auto [info, source] = kieli::test_info_and_source(std::move(string));

        kieli::Lex_state state {
            .compile_info = info,
            .source       = source,
            .string       = source->string(),
        };

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

#define TEST(name) TEST_CASE(name, "[liblex][trivia]") // NOLINT

TEST("whitespace trivia")
{
    REQUIRE(
        lex("\ta\nb  \t  c  \n  d\n\n e ")
        == "('\t' lower)"
           "('\n' lower)"
           "('  \t  ' lower)"
           "('  \n  ' lower)"
           "('\n\n ' lower)"
           "(' ' end of input)");

    REQUIRE(lex(" \t \n ") == "(' \t \n ' end of input)");
}

TEST("line comment trivia")
{
    REQUIRE(
        lex(" a // b \n c // d")
        == "(' ' lower)"
           "(' // b \n ' lower)"
           "(' // d' end of input)");
}

TEST("block commen trivia")
{
    REQUIRE(
        lex(". /* , /*::*/! */ in /**/ / //")
        == "('' .)"
           "(' /* , /*::*/! */ ' in)"
           "(' /**/ ' op)"
           "(' //' end of input)");

    REQUIRE(
        lex(R"(/* "" */ . /* "*/" */ . "/* /*" . /* /* "*/"*/ */ .)")
        == "('/* \"\" */ ' .)"
           "(' /* \"*/\" */ ' .)"
           "(' ' str)"
           "(' ' .)"
           "(' /* /* \"*/\"*/ */ ' .)"
           "('' end of input)");
}
