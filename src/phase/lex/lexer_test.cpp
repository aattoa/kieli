#include "utl/utilities.hpp"
#include "lex.hpp"
#include <catch2/catch_test_macros.hpp>


namespace {
    using Token = compiler::Lexical_token;
    using enum Token::Type;

    auto lex(std::string_view const text) -> std::vector<Token::Type> {
        return utl::map(&Token::type, compiler::lex({ .source = utl::Source { "[TEST] ", std::string { text }}}).tokens);
    }
    auto types(std::vector<Token::Type> vector) -> std::vector<Token::Type> {
        vector.push_back(end_of_input);
        return vector;
    }
}

#define TEST(name) TEST_CASE(name, "[lex]")


TEST("whitespace") {
    REQUIRE(lex("\ta\nb  \t  c  \n  d\n\n e ") == types(
        { lower_name
        , lower_name
        , lower_name
        , lower_name
        , lower_name }));
}


TEST("numeric") {
    REQUIRE(lex("23.4 1.") == types(
        { floating
        , floating }));

    REQUIRE(lex("50 0xdeadbeef -3 3e3 18446744073709551615") == types(
        { integer_of_unknown_sign
        , integer_of_unknown_sign
        , signed_integer
        , integer_of_unknown_sign
        , unsigned_integer }));

    REQUIRE(lex("0.3e-5 -0. -0.2E5") == types(
        { floating
        , floating
        , floating }));
}


TEST("tuple member access") {
    REQUIRE(lex(".0.0, 0.0") == types(
        { dot
        , integer_of_unknown_sign
        , dot
        , integer_of_unknown_sign
        , comma
        , floating }));
}


TEST("punctuation") {
    REQUIRE(lex("\n::\t,;(--? @#") == types(
        { double_colon
        , comma
        , semicolon
        , paren_open
        , operator_name
        , operator_name }));
}


TEST("comment") {
    REQUIRE(lex(". /* , /*::*/! */ in /**/ / //") == types(
        { dot
        , in
        , operator_name }));

    REQUIRE(lex(R"(/* "" */ . /* "*/" */ . "/* /*" . /* /* "*/"*/ */ .)") == types(
        { dot
        , dot
        , string
        , dot
        , dot }));
}


TEST("keyword") {
    REQUIRE(lex("for;forr(for2") == types(
        { for_
        , semicolon
        , lower_name
        , paren_open
        , lower_name }));

    REQUIRE(lex(",.[}\tmatch::") == types(
        { comma
        , dot
        , bracket_open
        , brace_close
        , match
        , double_colon }));
}


TEST("pattern") {
    REQUIRE(lex("x1 _ wasd,3") == types(
        { lower_name
        , underscore
        , lower_name
        , comma
        , integer_of_unknown_sign }));

    REQUIRE(lex("a<$>_:\nVec") == types(
        { lower_name
        , operator_name
        , underscore
        , colon
        , upper_name }));

    REQUIRE(lex("_, ______::_________________") == types(
        { underscore
        , comma
        , underscore
        , double_colon
        , underscore }));
}


TEST("string") {
    REQUIRE(lex("\"test\\t\\\",\", 'a', '\\\\'") == types( // NOLINT
        { string
        , comma
        , character
        , comma
        , character }));

    REQUIRE(lex(R"("hmm" ", yes")") == types(
        { string }));
}


TEST("casing") {
    REQUIRE(lex("a A _a _A _0 _") == types(
        { lower_name
        , upper_name
        , lower_name
        , upper_name
        , lower_name
        , underscore }));
}
