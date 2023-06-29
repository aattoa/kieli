#include <libutl/common/utilities.hpp>
#include <libparse/parse.hpp>
#include <libformat/format.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("parse-expression " name, "[libparse][expression]") // NOLINT
#define REQUIRE_SIMPLE_PARSE(string) REQUIRE(parse(string) == (string))

namespace {
    auto parse(std::string_view const string) -> std::string {
        auto const [info, source] = compiler::test_info_and_source(std::format("fn f() = {}", string));
        auto parse_result = kieli::parse(kieli::lex({ std::move(info), source }));
        REQUIRE(parse_result.module.definitions.size() == 1);
        auto output = kieli::format(parse_result.module, {
            .block_indentation = 1,
            .function_body     = kieli::Format_configuration::Function_body::leave_as_is,
        });
        output.pop_back(); // Remove the newline
        return output.substr(output.find('=') + 2); // +2 for the "= "
    }
}


TEST("integer literal") {
    REQUIRE_SIMPLE_PARSE("5");
    REQUIRE(parse("5e3") == "5000");
}

TEST("floating literal") {
    REQUIRE_SIMPLE_PARSE("5.0");
    REQUIRE(parse("5.0e3") == "5300.0");
}

TEST("boolean literal") {
    REQUIRE_SIMPLE_PARSE("true");
    REQUIRE_SIMPLE_PARSE("false");
}

TEST("character literal") {
    REQUIRE_SIMPLE_PARSE("'x'");
    REQUIRE_SIMPLE_PARSE("'\n'");
}

TEST("string literal") {
    REQUIRE_SIMPLE_PARSE("\"\"");
    REQUIRE_SIMPLE_PARSE("\"hello\"");
    REQUIRE_SIMPLE_PARSE("\"hello,\tworld!\n\"");
}

TEST("parenthesized") {
    REQUIRE_SIMPLE_PARSE("()");
    REQUIRE_SIMPLE_PARSE("(5)");
    REQUIRE_SIMPLE_PARSE("(5, 3)");
}

TEST("array literal") {
    REQUIRE_SIMPLE_PARSE("[]");
    REQUIRE_SIMPLE_PARSE("[5]");
    REQUIRE_SIMPLE_PARSE("[5; 3]");
}

TEST("self") {
    REQUIRE_SIMPLE_PARSE("self");
}

TEST("variable") {
    REQUIRE_SIMPLE_PARSE("x");
    REQUIRE_SIMPLE_PARSE("x::y");
    REQUIRE_SIMPLE_PARSE("x[]::y");
    REQUIRE_SIMPLE_PARSE("x[A, B]::y");
    REQUIRE_SIMPLE_PARSE("global::x");
    REQUIRE_SIMPLE_PARSE("global::x::y");
    REQUIRE_SIMPLE_PARSE("global::x[]::y");
    REQUIRE_SIMPLE_PARSE("global::x[A, B]::y");
}

TEST("template application") {
    REQUIRE_SIMPLE_PARSE("x[]");
    REQUIRE_SIMPLE_PARSE("x[A, B]");
    REQUIRE_SIMPLE_PARSE("x::y[]");
    REQUIRE_SIMPLE_PARSE("x::y[A, B]");
}

TEST("block") {
    REQUIRE_SIMPLE_PARSE("{}");
    REQUIRE_SIMPLE_PARSE("{ x }");
    REQUIRE(parse("{ x; y }") == "{\n x;\n y\n}");
    REQUIRE(parse("{ a; { b; c; }; d; { e; f } }") ==
        "{\n a;\n {\n b;\n c;\n};\n d;\n {\n  e;\n  f\n }\n}");
}

TEST("invocation") {
    REQUIRE_SIMPLE_PARSE("f()");
    REQUIRE_SIMPLE_PARSE("f(x, y)");
    REQUIRE_SIMPLE_PARSE("a::b()");
    REQUIRE_SIMPLE_PARSE("a::b(x, y)");
    REQUIRE_SIMPLE_PARSE("(a.b)()");
    REQUIRE_SIMPLE_PARSE("(a.b)(x, y)");
}

TEST("method invocation") {
    REQUIRE_SIMPLE_PARSE("a.b()");
    REQUIRE_SIMPLE_PARSE("a.b(x, y)");
    REQUIRE_SIMPLE_PARSE("a::b.c()");
    REQUIRE_SIMPLE_PARSE("a::b.c(x, y)");
}

TEST("struct initializer") {
    REQUIRE_SIMPLE_PARSE("S {}");
    REQUIRE_SIMPLE_PARSE("S { x = 10, y = \"hello\" }");
    REQUIRE_SIMPLE_PARSE("A::B {}");
    REQUIRE_SIMPLE_PARSE("typeof(x) {}");
    REQUIRE_SIMPLE_PARSE("typeof(x)::T {}");
}

TEST("binary operator invocation") {
    REQUIRE_SIMPLE_PARSE("a * b");
    REQUIRE_SIMPLE_PARSE("a <$> b");
    REQUIRE_SIMPLE_PARSE("a * b + c");
    REQUIRE_SIMPLE_PARSE("a *** (a //// b) +++ c");
}

TEST("struct field access") {
    REQUIRE_SIMPLE_PARSE("a.b");
    REQUIRE_SIMPLE_PARSE("a.b.c");
}

TEST("tuple field access") {
    REQUIRE_SIMPLE_PARSE("x.0");
    REQUIRE_SIMPLE_PARSE("x.0.1");
}

TEST("array field access") {
    REQUIRE_SIMPLE_PARSE("x.[y]");
    REQUIRE_SIMPLE_PARSE("x.[y].[z]");
}

TEST("conditional") {
    REQUIRE_SIMPLE_PARSE("if a { b }");
    REQUIRE_SIMPLE_PARSE("if a { b } else { c } ");
    REQUIRE_SIMPLE_PARSE("if a { b } elif c { d } else { e }");
    REQUIRE_SIMPLE_PARSE("if let a = b { c }");
    REQUIRE_SIMPLE_PARSE("if let a = b { c } else { d }");
    REQUIRE_SIMPLE_PARSE("if let a = b { c } elif let d = e { f } else { g }");
}

TEST("match") {
    REQUIRE(parse("match a { b -> c d -> e }") ==
        "match a {\n b -> c\n d -> e\n}");
}

TEST("type cast") {
    REQUIRE_SIMPLE_PARSE("x as X");
    REQUIRE_SIMPLE_PARSE("a as B as C");
}

TEST("type ascription") {
    REQUIRE_SIMPLE_PARSE("x: X");
    REQUIRE_SIMPLE_PARSE("a: B: C");
}

TEST("let binding") {
    REQUIRE_SIMPLE_PARSE("let x = y");
    REQUIRE_SIMPLE_PARSE("let x: T = y");
    REQUIRE_SIMPLE_PARSE("let (a, b) = x");
    REQUIRE_SIMPLE_PARSE("let (a, b): (A, B) = x");
}

TEST("type alias") {
    REQUIRE_SIMPLE_PARSE("alias T = I32");
}

TEST("infinite loop") {
    REQUIRE_SIMPLE_PARSE("loop {}");
}

TEST("while loop") {
    REQUIRE_SIMPLE_PARSE("while x { y }");
    REQUIRE_SIMPLE_PARSE("while let x = y { z }");
}

TEST("for loop") {
    REQUIRE_SIMPLE_PARSE("for x in xs {}");
    REQUIRE_SIMPLE_PARSE("for (x, y) in [(10, 'x'); (20, 'y')] {}");
}

TEST("loop directives") {
    REQUIRE_SIMPLE_PARSE("continue");
    REQUIRE_SIMPLE_PARSE("break");
    REQUIRE_SIMPLE_PARSE("break 5");
}

TEST("discard") {
    REQUIRE_SIMPLE_PARSE("discard x");
    REQUIRE_SIMPLE_PARSE("discard (x)");
    REQUIRE_SIMPLE_PARSE("discard {}");
    REQUIRE_SIMPLE_PARSE("discard { x }");
}

TEST("ret") {
    REQUIRE_SIMPLE_PARSE("ret");
    REQUIRE_SIMPLE_PARSE("ret x");
}

TEST("reference") {
    REQUIRE_SIMPLE_PARSE("&x");
    REQUIRE_SIMPLE_PARSE("&mut x");
    REQUIRE_SIMPLE_PARSE("&x.y");
    REQUIRE_SIMPLE_PARSE("&mut x.y");
}

TEST("sizeof") {
    REQUIRE_SIMPLE_PARSE("sizeof(T)");
    REQUIRE_SIMPLE_PARSE("sizeof((A, B))");
    REQUIRE_SIMPLE_PARSE("sizeof(a::b::C)");
}

TEST("addressof") {
    REQUIRE_SIMPLE_PARSE("addressof(x)");
    REQUIRE_SIMPLE_PARSE("addressof(x.y)");
}

TEST("reference dereference") {
    REQUIRE_SIMPLE_PARSE("*x");
    REQUIRE_SIMPLE_PARSE("*x.y");
}

TEST("pointer dereference") {
    REQUIRE_SIMPLE_PARSE("dereference(x)");
    REQUIRE_SIMPLE_PARSE("dereference(x.y)");
}

TEST("unsafe") {
    REQUIRE_SIMPLE_PARSE("unsafe {}");
    REQUIRE_SIMPLE_PARSE("unsafe { x }");
    REQUIRE(parse("unsafe { x; y }") == "unsafe {\n x;\n y\n}");
}

TEST("mov") {
    REQUIRE_SIMPLE_PARSE("mov x");
    REQUIRE_SIMPLE_PARSE("mov x.y");
    REQUIRE_SIMPLE_PARSE("mov x.[y]");
}

TEST("meta") {
    REQUIRE_SIMPLE_PARSE("meta(5)");
}

TEST("hole") {
    REQUIRE_SIMPLE_PARSE("\?\?\?");
}
