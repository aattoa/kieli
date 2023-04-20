#include "utl/utilities.hpp"
#include "phase/lex/lex.hpp"
#include <catch2/catch_test_macros.hpp>

using Token = compiler::Lexical_token;

namespace {
    auto lex(std::string&& string) -> std::vector<Token::Type> {
        compiler::Compilation_info test_info = compiler::mock_compilation_info();
        utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));
        return utl::map(&Token::type, compiler::lex({ .compilation_info = std::move(test_info), .source = test_source}).tokens);
    }
    auto types(std::vector<Token::Type> vector) -> std::vector<Token::Type> {
        vector.push_back(Token::Type::end_of_input);
        return vector;
    }
}

#define TEST(name) TEST_CASE(name, "[lex]")


TEST("whitespace") {
    REQUIRE(lex("\ta\nb  \t  c  \n  d\n\n e ") == types(
        { Token::Type::lower_name
        , Token::Type::lower_name
        , Token::Type::lower_name
        , Token::Type::lower_name
        , Token::Type::lower_name }));
}


TEST("numeric") {
    REQUIRE(lex("23.4 1.") == types(
        { Token::Type::floating
        , Token::Type::floating }));

    REQUIRE(lex("50 0xdeadbeef -3 3e3 18446744073709551615") == types(
        { Token::Type::integer_of_unknown_sign
        , Token::Type::integer_of_unknown_sign
        , Token::Type::signed_integer
        , Token::Type::integer_of_unknown_sign
        , Token::Type::unsigned_integer }));

    REQUIRE(lex("0.3e-5 -0. -0.2E5") == types(
        { Token::Type::floating
        , Token::Type::floating
        , Token::Type::floating }));
}


TEST("tuple member access") {
    REQUIRE(lex(".0.0, 0.0") == types(
        { Token::Type::dot
        , Token::Type::integer_of_unknown_sign
        , Token::Type::dot
        , Token::Type::integer_of_unknown_sign
        , Token::Type::comma
        , Token::Type::floating }));
}


TEST("punctuation") {
    REQUIRE(lex("\n::\t,;(--? @#") == types(
        { Token::Type::double_colon
        , Token::Type::comma
        , Token::Type::semicolon
        , Token::Type::paren_open
        , Token::Type::operator_name
        , Token::Type::operator_name }));
}


TEST("comment") {
    REQUIRE(lex(". /* , /*::*/! */ in /**/ / //") == types(
        { Token::Type::dot
        , Token::Type::in
        , Token::Type::operator_name }));

    REQUIRE(lex(R"(/* "" */ . /* "*/" */ . "/* /*" . /* /* "*/"*/ */ .)") == types(
        { Token::Type::dot
        , Token::Type::dot
        , Token::Type::string
        , Token::Type::dot
        , Token::Type::dot }));
}


TEST("keyword") {
    REQUIRE(lex("for;forr(for2") == types(
        { Token::Type::for_
        , Token::Type::semicolon
        , Token::Type::lower_name
        , Token::Type::paren_open
        , Token::Type::lower_name }));

    REQUIRE(lex(",.[}\tmatch::") == types(
        { Token::Type::comma
        , Token::Type::dot
        , Token::Type::bracket_open
        , Token::Type::brace_close
        , Token::Type::match
        , Token::Type::double_colon }));
}


TEST("pattern") {
    REQUIRE(lex("x1 _ wasd,3") == types(
        { Token::Type::lower_name
        , Token::Type::underscore
        , Token::Type::lower_name
        , Token::Type::comma
        , Token::Type::integer_of_unknown_sign }));

    REQUIRE(lex("a<$>_:\nVec") == types(
        { Token::Type::lower_name
        , Token::Type::operator_name
        , Token::Type::underscore
        , Token::Type::colon
        , Token::Type::upper_name }));

    REQUIRE(lex("_, ______::_________________") == types(
        { Token::Type::underscore
        , Token::Type::comma
        , Token::Type::underscore
        , Token::Type::double_colon
        , Token::Type::underscore }));
}


TEST("string") {
    REQUIRE(lex("\"test\\t\\\",\", 'a', '\\\\'") == types( // NOLINT
        { Token::Type::string
        , Token::Type::comma
        , Token::Type::character
        , Token::Type::comma
        , Token::Type::character }));

    REQUIRE(lex(R"("hmm" ", yes")") == types(
        { Token::Type::string }));
}


TEST("casing") {
    REQUIRE(lex("a A _a _A _0 _") == types(
        { Token::Type::lower_name
        , Token::Type::upper_name
        , Token::Type::lower_name
        , Token::Type::upper_name
        , Token::Type::lower_name
        , Token::Type::underscore }));
}
