#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    auto lex(std::string&& string) -> std::string
    {
        compiler::Compilation_info test_info = compiler::mock_compilation_info();
        utl::wrapper auto const    test_source
            = test_info.get()->source_arena.wrap("[test]", std::move(string));
        auto const lex_result
            = kieli::lex({ .compilation_info = std::move(test_info), .source = test_source });

        std::string output;
        for (auto const& token : lex_result.tokens) {
            std::format_to(
                std::back_inserter(output), "('{}' {})", token.preceding_trivia, token.type);
        }
        return output;
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
