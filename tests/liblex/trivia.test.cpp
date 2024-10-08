#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include <cppunittest/unittest.hpp>

namespace {
    auto lex(std::string&& text) -> std::string
    {
        auto       db          = kieli::Database {};
        auto const document_id = kieli::test_document(db, std::move(text));
        auto       state       = kieli::lex_state(db, document_id);

        std::string output;
        for (;;) {
            auto token = kieli::lex(state);
            std::format_to(
                std::back_inserter(output), "('{}' {})", token.preceding_trivia, token.type);
            if (token.type == kieli::Token_type::end_of_input) {
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
        "(' ' eof)");

    CHECK_EQUAL(lex(" \t \n "), "(' \t \n ' eof)");
}

TEST("line comment trivia")
{
    CHECK_EQUAL(
        lex(" a // b \n c // d"),
        "(' ' lower)"
        "(' // b \n ' lower)"
        "(' // d' eof)");
}

TEST("block commen trivia")
{
    CHECK_EQUAL(
        lex(". /* , /*::*/! */ in /**/ / //"),
        "('' .)"
        "(' /* , /*::*/! */ ' in)"
        "(' /**/ ' op)"
        "(' //' eof)");

    CHECK_EQUAL(
        lex(R"(/* "" */ . /* "*/" */ . "/* /*" . /* /* "*/"*/ */ .)"),
        "('/* \"\" */ ' .)"
        "(' /* \"*/\" */ ' .)"
        "(' ' str)"
        "(' ' .)"
        "(' /* /* \"*/\"*/ */ ' .)"
        "('' eof)");
}
