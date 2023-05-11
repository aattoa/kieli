#include <libutl/common/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>


namespace {
    auto do_resolve(std::string&& string, utl::diagnostics::Level const diagnostics_level) -> compiler::Resolve_result {
        compiler::Compilation_info test_info = compiler::mock_compilation_info(diagnostics_level);
        utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));
        auto lex_result = compiler::lex({ .compilation_info = std::move(test_info), .source = test_source });
        return compiler::resolve(compiler::desugar(compiler::parse(std::move(lex_result))));
    }
    auto resolve(std::string&& string) -> std::string {
        auto resolve_result = do_resolve(std::move(string), utl::diagnostics::Level::suppress);
        std::string output;
        for (utl::wrapper auto const wrapper : resolve_result.module.functions) {
            auto& function = utl::get<mir::Function>(wrapper->value);
            if (function.signature.is_template()) continue;
            fmt::format_to(std::back_inserter(output), "{}", function);
        }
        return output;
    }
    auto resolution_diagnostics(std::string&& string) -> std::string {
        auto resolve_result = do_resolve(std::move(string), utl::diagnostics::Level::normal);
        return std::move(resolve_result.compilation_info.get()->diagnostics).string();
    }
    auto contains(std::string const& string) {
        return Catch::Matchers::ContainsSubstring(string, Catch::CaseSensitive::No);
    }
}

#define TEST(name) TEST_CASE(name, "[resolve]") // NOLINT
#define REQUIRE_RESOLUTION_SUCCESS(expression) REQUIRE_NOTHROW((void)(expression))
#define REQUIRE_RESOLUTION_FAILURE(expression, error_string) \
    REQUIRE_THROWS_MATCHES((void)(expression), utl::diagnostics::Error, Catch::Matchers::MessageMatches(contains(error_string)))


TEST("name resolution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = x"),                           "no definition for 'x' in scope");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = test::f()"),                   "no definition for 'test' in scope");
    REQUIRE_RESOLUTION_FAILURE(resolve("namespace test {} fn f() = test::f()"), "test does not contain a definition for 'f'");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = ::g()"),                       "the global namespace does not contain a definition for 'g'");
    REQUIRE(resolve(
        "namespace a {"
            "namespace b { fn f() = g() }"
            "fn g() = 5: I64"
        "}")
        ==
        "fn f(): I64 = ({ (g()): I64 }): I64"
        "fn g(): I64 = ({ (5): I64 }): I64");
    REQUIRE(resolve(
        "namespace test { fn f(): I32 = ??? } fn f() = (test::f(), ())")
        ==
        "fn f(): I32 = ({ (\?\?\?): I32 }): I32"
        "fn f(): (I32, ()) = ({ (((f()): I32, (()): ())): (I32, ()) }): (I32, ())");
}

TEST("scope") {
    REQUIRE_THAT(resolution_diagnostics("fn f() { let x = \?\?\?; }"),                 contains("unused local variable"));
    REQUIRE_THAT(resolution_diagnostics("fn f() { let x = \?\?\?; let x = \?\?\?; }"), contains("shadows an unused local variable"));
    REQUIRE(resolve(
        "fn f() {"
            "let x = 3.14;"
            "let x = \"hello\";"
            "let x = (x, x);"
        "}")
        ==
        "fn f(): () = ({ "
            "(let x: Float = (3.14): Float): (); "
            "(let x: String = (\"hello\"): String): (); "
            "(let x: (String, String) = (((x): String, (x): String)): (String, String)): (); "
            "(()): () "
        "}): ()");
}

TEST("mutability") {
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { let mut x = ' '; &mut x }"));
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f[m: mut]() { let mut?m x = ' '; &mut?m x }"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = ' '; &mut x }"),               "acquire mutable reference");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[m: mut]() { let mut?m x = ' '; &mut x }"), "acquire mutable reference");

    REQUIRE(resolve(
        "fn f() {"
            "let x = 3.14;"
            "let y = &x;"
            "let _ = &(*y)"
        "}")
        ==
        "fn f(): () = ({ "
            "(let x: Float = (3.14): Float): (); "
            "(let y: &Float = (&(x): Float): &Float): (); "
            "(let _: &Float = (&(*(y): &Float): Float): &Float): () "
        "}): ()");

    REQUIRE_RESOLUTION_FAILURE(resolve(
        "fn f() {"
            "let x = 3.14;"
            "let y = &x;"
            "let _ = &mut (*y)"
        "}"),
        "acquire mutable reference");

    REQUIRE(resolve(
        "fn f() { let a = \?\?\?; let _: &I32 = &(*a); }") ==
        "fn f(): () = ({ (let a: &I32 = (\?\?\?): &I32): (); (let _: &I32 = (&(*(a): &I32): I32): &I32): (); (()): () }): ()");
    REQUIRE(resolve(
        "fn f() { let a = \?\?\?; let _: &mut I32 = &mut (*a); }") ==
        "fn f(): () = ({ (let a: &mut I32 = (\?\?\?): &mut I32): (); (let _: &mut I32 = (&mut (*(a): &mut I32): I32): &mut I32): (); (()): () }): ()");
    REQUIRE(resolve(
        "fn f() { let a = \?\?\?; let b = &mut *a; let _: Char = *b; }") ==
        "fn f(): () = ({ (let a: &mut Char = (\?\?\?): &mut Char): (); (let b: &mut Char = (&mut (*(a): &mut Char): Char): &mut Char): (); (let _: Char = (*(b): &mut Char): Char): (); (()): () }): ()");
}

TEST("return type resolution") {
    REQUIRE(resolve("fn f() = 5: I32") == "fn f(): I32 = ({ (5): I32 }): I32");
    REQUIRE(resolve("fn g() = \"hello\"") == "fn g(): String = ({ (\"hello\"): String }): String");
    REQUIRE(resolve("fn f(): U8 = 5") == "fn f(): U8 = ({ (5): U8 }): U8");
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f(): I32 = f()"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = f()"), "circular dependency");
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

TEST("reference mutability coercion") {
    REQUIRE(resolve("fn f() { let mut x: U8 = 5; let _: &mut U8 = &mut x; }") ==
        "fn f(): () = ({ (let mut x: U8 = (5): U8): (); (let _: &mut U8 = (&mut (x): U8): &mut U8): (); (()): () }): ()");
    REQUIRE(resolve("fn f() { let mut x: U8 = 5; let _: &U8 = &mut x; }") ==
        "fn f(): () = ({ (let mut x: U8 = (5): U8): (); (let _: &U8 = (&mut (x): U8): &mut U8): (); (()): () }): ()");
}

TEST("double variable solution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = 5; let _: (I32, I64) = (x, x); }"),        "initializer is of type");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = \?\?\?; let _: (String, I8) = (x, x); }"), "initializer is of type");
}

TEST("struct initializer") {
    REQUIRE(resolve("struct S = a: I32, b: I64 fn f() = S { a = 10, b = 5 }")
        == "fn f(): S = ({ (S { (10): I32, (5): I64 }): S }): S");
    REQUIRE(resolve("struct S = a: I32, b: I64 fn f() = S { b = 10, a = 5 }")
        == "fn f(): S = ({ (S { (5): I32, (10): I64 }): S }): S");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { a = ' ' }"),             "initializer is of type Char");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { a = 10 }"),              "'b' is not initialized");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { b = 10 }"),              "'a' is not initialized");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = a: I32, b: I64 fn f() = S { a = 0, b = 0, c = 0 }"), "S does not have");
}

TEST("loop resolution") {
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { loop { break; } }"));
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { loop { continue; } }"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { break; }"),    "can not appear outside of a loop");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { continue; }"), "can not appear outside of a loop");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { while \?\?\? { break \"\"; } }"),  "non-unit type");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { loop { break \"\"; break 5; } }"), "previous break expressions had results of type String");

    REQUIRE_THAT(resolution_diagnostics("fn f() = while true {}"),  contains("'loop' instead of 'while true'"));
    REQUIRE_THAT(resolution_diagnostics("fn f() = while false {}"), contains("will never be run"));

    REQUIRE(resolve("fn f() = while \?\?\? {}") ==
        "fn f(): () = ({ "
            "(loop (if (\?\?\?): Bool "
                "({ (()): () }): () "
            "else "
                "(break (()): ()): ()): ()): ()"
        " }): ()");
}

TEST("template argument resolution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[T](): T = ??? fn g() = f[]()"),                         "requires exactly 1 template argument, but 0 were supplied");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[A, B](): (A, B) = ??? fn g() = f[I8]()"),               "requires exactly 2 template arguments, but 1 was supplied");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[A, B](): (A, B) = ??? fn g() = f[I8, I16, I32]()"),     "requires exactly 2 template arguments, but 3 were supplied");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[A, B=I64](): (A, B) = ??? fn g() = f[I8, I16, I32]()"), "has only 2 template parameters, but 3 template arguments were supplied");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f[A, B, C=B](): (A, B, C) = ??? fn g() = f[I8]()"),       "requires at least 2 template arguments, but 1 was supplied");
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f[A, B=I64](): (A, B) = ??? fn g() = f[I8]()"));
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f[A, B=A](): (A, B) = ??? fn g() = f[I8]()"));
    REQUIRE_RESOLUTION_SUCCESS(resolve("namespace test { struct S = s: I64 fn f[A, B=S](): (A, B) = ??? } fn g() = test::f[I8]()"));
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
        "the argument is of type Char");
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
        "fn g() = S[] { a = 3: I32, b = \"bbb\" }")
        ==
        "fn f(): S[String, Float] = ({ "
            "(S[String, Float] { (\"aaa\"): String, (2.74): Float }): S[String, Float]"
        " }): S[String, Float]"
        "fn g(): S[I32, String] = ({ "
            "(S[I32, String] { (3): I32, (\"bbb\"): String }): S[I32, String]"
        " }): S[I32, String]");
}

TEST("simple method lookup") {
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

TEST("function generalization") {
    REQUIRE(resolve(
        "fn f() = ??? "
        "fn g(): String = f() "
        "fn h(): I32 = f()")
        ==
        "fn g(): String = ({ (f[String]()): String }): String"
        "fn h(): I32 = ({ (f[I32]()): I32 }): I32");

    REQUIRE(resolve(
        "fn f(x: _) = x "
        "fn g() = f(5: U8) "
        "fn h() = f(\"hello\")")
        ==
        "fn g(): U8 = ({ (f[U8]((5): U8)): U8 }): U8"
        "fn h(): String = ({ (f[String]((\"hello\"): String)): String }): String");

    REQUIRE(resolve(
        "fn f(x: _, y: typeof(x)) = (x, y)"
        "fn g() = f(\?\?\?, 3.14)")
        ==
        "fn g(): (Float, Float) = ({ "
            "(f[Float]((\?\?\?): Float, (3.14): Float)): (Float, Float)"
        " }): (Float, Float)");

    REQUIRE(resolve(
        "fn f(x: _, y: typeof(x)) = (x, y)"
        "fn g(): (String, String) = f(\?\?\?, \?\?\?)")
        ==
        "fn g(): (String, String) = ({ "
            "(f[String]((\?\?\?): String, (\?\?\?): String)): (String, String)"
        " }): (String, String)");

    REQUIRE_RESOLUTION_FAILURE(resolve(
        "fn f(x: _, y: typeof(x)) = (x, y)"
        "fn g() = f(5: U8, 3.14)"),
        "but the argument is of type Float");

    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = x: typeof(\?\?\?)"), "contains an unsolved");
    REQUIRE_RESOLUTION_FAILURE(resolve("struct S = x: _"),              "contains an unsolved");
    REQUIRE_RESOLUTION_FAILURE(resolve("enum E = e(_)"),                "contains an unsolved");
    REQUIRE_RESOLUTION_FAILURE(resolve("alias A = _"),                  "contains an unsolved");
    REQUIRE_RESOLUTION_FAILURE(resolve("class C { fn f(_: _): I32 }"),  "contains an unsolved");
    REQUIRE_RESOLUTION_FAILURE(resolve("class C { fn f(_: I32): _ }"),  "contains an unsolved");
}
