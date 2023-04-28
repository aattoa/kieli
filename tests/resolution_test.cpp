#include "utl/utilities.hpp"
#include "phase/resolve/resolve.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>


namespace {
    auto resolve(std::string&& string) -> std::string {
        compiler::Compilation_info test_info = compiler::mock_compilation_info();
        utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));
        auto lex_result = compiler::lex({ .compilation_info = std::move(test_info), .source = test_source });
        auto resolve_result = compiler::resolve(compiler::desugar(compiler::parse(std::move(lex_result))));

        std::string output;
        for (utl::wrapper auto const function : resolve_result.module.functions)
            fmt::format_to(std::back_inserter(output), "{}", std::get<mir::Function>(function->value));
        return output;
    }
}

#define TEST(name) TEST_CASE(name, "[resolve]")
#define REQUIRE_RESOLUTION_SUCCESS(expression) REQUIRE_NOTHROW((void)expression)
#define REQUIRE_RESOLUTION_FAILURE(expression, errorstring) \
    REQUIRE_THROWS_MATCHES((void)expression, utl::diagnostics::Error, \
    Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring(errorstring, Catch::CaseSensitive::No)))


TEST("name resolution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = x"), "no definition for");
    // TODO: namespace access
}

TEST("mutability") {
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { let mut x = ' '; &mut x }"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = ' '; &mut x }"), "mutable reference");
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f[m: mut]() { let mut?m x = ' '; &mut?m x }"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[m: mut]() { let mut?m x = ' '; &mut x }"), "mutable reference");
}

TEST("return type deduction") {
    REQUIRE(resolve("fn f() = 5: I32") == "fn f(): I32 = ({ (5): I32 }): I32");
    REQUIRE(resolve("fn g() = \"hello\"") == "fn g(): String = ({ (\"hello\"): String }): String");
}

TEST("return type unification") {
    REQUIRE(resolve("fn f(): U8 = 5") == "fn f(): U8 = ({ (5): U8 }): U8");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f(): U8 = 5: I8"), "the body is of type I8");
}

TEST("local unification") {
    REQUIRE(resolve("fn f() { let x = ???; let f: fn(String): I64 = \?\?\?; f(x) }") ==
        "fn f(): I64 = ({ "
            "(let x: String = (\?\?\?): String): (); "
            "(let f: fn(String): I64 = (\?\?\?): fn(String): I64): (); "
            "((f): fn(String): I64((x): String)): I64"
        " }): I64");
}

TEST("match case unification") {
    REQUIRE(resolve(
        "enum Option[T] = none | some(T) "
        "fn f() { match Option::none { Option::some(x) -> x Option::none -> \"hello\" } }")
        ==
        "fn f(): String = ({ "
            "(match (Option[String]::none): Option[String] { "
                "Option[String]::some(x) -> (x): String "
                "Option[String]::none -> (\"hello\"): String"
            " }): String"
        " }): String");
}

TEST("pointer unification") {
    REQUIRE(resolve("fn f(): Char { let x = \?\?\?; unsafe_dereference(addressof(x)) }")
    ==
    "fn f(): Char = ({ "
        "(let x: Char = (\?\?\?): Char): (); "
        "(unsafe_dereference((addressof((x): Char)): *Char)): Char"
    " }): Char");
}

TEST("template argument deduction") {
    REQUIRE(resolve(
        "fn f[T](c: Bool, a: T, b: T) = if c { a } else { b }"
        "fn g() = f(true, 3.14, 2.74)"
        "fn h(): I32 = f(false, 10, 20)")
        ==
        "fn g(): Float = ({ "
            "(f[Float]((true): Bool, (3.14): Float, (2.74): Float)): Float"
        " }): Float"
        "fn h(): I32 = ({ "
            "(f[I32]((false): Bool, (10): I32, (20): I32)): I32"
        " }): I32");
}

TEST("double variable solution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = 5; let _: (I32, I64) = (x, x); }"),
        "initializer is of type");
}

TEST("multiple template instantiations") {
    REQUIRE(resolve(
        "enum Option[T] = none | some(T) "
        "fn get[T](_: Option[T]): T = \?\?\?"
        "fn f(): String { let o = \?\?\?; get(o) }"
        "fn g(): I64 { let o = \?\?\?; get(o) }")
        ==
        "fn f(): String = ({ "
            "(let o: Option[String] = (\?\?\?): Option[String]): (); "
            "(get[String]((o): Option[String])): String"
        " }): String"
        "fn g(): I64 = ({ "
            "(let o: Option[I64] = (\?\?\?): Option[I64]): (); "
            "(get[I64]((o): Option[I64])): I64"
        " }): I64");
}

TEST("deduce from invocation") {
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Option[T] = none | some(T) "
        "fn new[T](): Option[T] = \?\?\? "
        "fn set[T](_: &mut Option[T], _: T) = ()"
        "fn f() { let mut x = new(); set(&mut x, 3.14); set(&mut x, ' '); }"),
        "the argument is of type &mut Option[Float]");
    REQUIRE(resolve(
        "enum Option[T] = none | some(T) "
        "fn new[T](): Option[T] = \?\?\? "
        "fn set[T](_: &mut Option[T], _: T) = ()"
        "fn f() { let mut x = new(); set(&mut x, 3.14); x }")
        ==
        "fn f(): Option[Float] = ({ "
            "(let mut x: Option[Float] = (new[Float]()): Option[Float]): (); "
            "(set[Float]((&mut (x): Option[Float]): &mut Option[Float], (3.14): Float)): (); "
            "(x): Option[Float]"
        " }): Option[Float]");
}

TEST("struct initializer") {
    REQUIRE(resolve("struct S = a: I32, b: I64 fn f() = S { a = 10, b = 5 }")
        == "fn f(): S = ({ (S { (10): I32, (5): I64 }): S }): S");
    REQUIRE(resolve("struct S = a: I32, b: I64 fn f() = S { b = 10, a = 5 }")
        == "fn f(): S = ({ (S { (5): I32, (10): I64 }): S }): S");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { a = ' ' }"),
        "the given initializer is of type Char");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { a = 10 }"),
        "'b' is not initialized");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { b = 10 }"),
        "'a' is not initialized");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { a = 0, b = 0, c = 0 }"),
        "S does not have");
}

TEST("default template arguments") {
    REQUIRE(resolve(
        "struct Triple[A, B = A, C = B] = a: A, b: B, c: C "
        "fn f() = Triple[I32] { a = 0, b = 1, c = 2 }")
        ==
        "fn f(): Triple[I32, I32, I32] = ({ "
            "(Triple[I32, I32, I32] { (0): I32, (1): I32, (2): I32 }): Triple[I32, I32, I32]"
        " }): Triple[I32, I32, I32]");
    REQUIRE(resolve(
        "struct Triple[A, B = A, C = B] = a: A, b: B, c: C "
        "fn f() = Triple[I32, String] { a = \?\?\?, b = \?\?\?, c = \?\?\? }")
        ==
        "fn f(): Triple[I32, String, String] = ({ "
            "(Triple[I32, String, String] { (\?\?\?): I32, (\?\?\?): String, (\?\?\?): String }): Triple[I32, String, String]"
        " }): Triple[I32, String, String]");
}

TEST("wildcard template arguments") {
    REQUIRE(resolve(
        "struct S[A, B] = a: A, b: B "
        "fn f() = S[_, _] { a = \"aaa\", b = 2.74 }"
        "fn g() = S[_, _] { a = 2.74, b = \"aaa\" }")
        ==
        "fn f(): S[String, Float] = ({ "
            "(S[String, Float] { (\"aaa\"): String, (2.74): Float }): S[String, Float]"
        " }): S[String, Float]"
        "fn g(): S[Float, String] = ({ "
            "(S[Float, String] { (2.74): Float, (\"aaa\"): String }): S[Float, String]"
        " }): S[Float, String]");
    REQUIRE(resolve(
        "struct S[A = _, B = _] = a: A, b: B "
        "fn f() = S[] { a = \"aaa\", b = 2.74 } "
        "fn g() = S[I32, String] { a = 3, b = \"bbb\" }") // FIXME: remove explicit template arguments
        ==
        "fn f(): S[String, Float] = ({ "
            "(S[String, Float] { (\"aaa\"): String, (2.74): Float }): S[String, Float]"
        " }): S[String, Float]"
        "fn g(): S[I32, String] = ({ "
            "(S[I32, String] { (3): I32, (\"bbb\"): String }): S[I32, String]"
        " }): S[I32, String]");
}

TEST("method") {
    REQUIRE(resolve(
        "struct S = x: Char "
        "impl S { "
            "fn a[m: mut](&mut?m self) = &mut?m (*self).x "
            "fn b(&self): Char = (*self).x "
        "} "
        "fn f(s: S): &Char { "
            "discard s.b(); "
            "s.a()"
        "}")
        ==
        "fn f(s: S): &Char = ({ "
            "({ (let _: Char = (b((&(s): S): &S)): Char): (); (()): () }): (); "
            "(a[immut]((&(s): S): &S)): &Char"
        " }): &Char");
    REQUIRE(resolve(
        "struct S = x: Char "
        "impl S { fn f[T](&self): T = ??? }"
        "fn g[T]() {"
            "let x: S = ???;"
            "x.f[T]()"
        "}"
        "fn h(): Float = g()")
        ==
        "fn h(): Float = ({ (g[Float]()): Float }): Float");
}

TEST("map option") {
    REQUIRE(resolve(
        "enum Option[T] = none | some(T) "
        "fn get[T](o: Option[T]): T = ??? "
        "fn map[A, B](o: Option[A], f: fn(A): B): Option[B] = match o { "
            "Option::some(x) -> Option::some(f(x))"
            "Option::none -> Option::none"
        "}"
        "fn f(o: Option[I32]): String = get(map(o, \?\?\?))")
        ==
        "fn f(o: Option[I32]): String = ({ "
            "(get[String]((map[I32, String]((o): Option[I32], (\?\?\?): fn(I32): String)): Option[String])): String"
        " }): String");
}
