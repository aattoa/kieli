#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include "resolution_test.hpp"


namespace {
    auto contains(std::string const& string) {
        return Catch::Matchers::ContainsSubstring(string, Catch::CaseSensitive::No);
    }
    auto resolve(std::string&& string) -> std::string {
        return libresolve::do_test_resolve(std::move(string)).formatted_hir_functions;
    }
    auto resolution_diagnostics(std::string&& string) -> std::string {
        return libresolve::do_test_resolve(std::move(string)).diagnostics_messages;
    }
}


#define TEST(name) TEST_CASE("resolve " name, "[libresolve]") // NOLINT
#define REQUIRE_RESOLUTION_SUCCESS(expression) REQUIRE_NOTHROW((void)(expression))
#define REQUIRE_RESOLUTION_FAILURE(expression, error_string) \
    REQUIRE_THROWS_MATCHES((void)(expression), std::exception, Catch::Matchers::MessageMatches(contains(error_string)))


TEST("name resolution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = x"),                           "no definition for 'x' in scope");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = test::f()"),                   "no definition for 'test' in scope");
    REQUIRE_RESOLUTION_FAILURE(resolve("namespace test {} fn f() = test::f()"), "test does not contain a definition for 'f'");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() = global::g()"),                 "the global namespace does not contain a definition for 'g'");
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

TEST("pattern exhaustiveness checking") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f(5: I32) {}"),                        "inexhaustive");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let 5: I32 = ???; }"),           "inexhaustive");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let ((a, b), (5, d)) = ???; }"), "inexhaustive");
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { let ((a, b), (c, d)) = ???; }"));
}

TEST("let binding resolution") {
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { if let 5 = ??? {}; }"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = x; }"), "no definition for 'x' in scope");
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { let x = ???; }"));
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { let x: _ = ???; }"));
    REQUIRE_RESOLUTION_SUCCESS(resolve("fn f() { let x: typeof(x) = ???; }"));
    REQUIRE(resolve("fn f() { let x: typeof(x) = ???; }") == resolve("fn f() { let x = ???; }"));
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x: typeof((x, x)) = ???; }"), "recursive unification variable solution");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Option[T] = none | some(T)"
        "fn f() { let _: Option[I32] = 5: I32; }"),
        "Could not unify Option[I32] ~ I32");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Test[T] = test "
        "fn f() { let Test[I32]::test = 5: I32; }"),
        "Could not unify Test[I32] ~ I32");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Option[T] = none | some(T)"
        "fn f() { let Option[I32]::none = Option[I32]::some(5: I32); }"),
        "inexhaustive");
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
            "(let immut x: Float = (3.14): Float): (); "
            "(let immut x: String = (\"hello\"): String): (); "
            "(let immut x: (String, String) = (((x): String, (x): String)): (String, String)): (); "
            "(()): () "
        "}): ()");
}

TEST("safety status") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f(x: *I32): I32 { dereference(x) }"),
        "may not appear within safe context");
    REQUIRE(resolve("fn f(x: *I32): I32 { unsafe { dereference(x) } }") ==
        "fn f(immut x: *immut I32): I32 = ({ "
            "({ (dereference((x): *immut I32)): I32 }): I32"
        " }): I32");
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
            "(let immut x: Float = (3.14): Float): (); "
            "(let immut y: &immut Float = (&immut (x): Float): &immut Float): (); "
            "(let _: &immut Float = (&immut (*(y): &immut Float): Float): &immut Float): () "
        "}): ()");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "fn f() {"
            "let x = 3.14;"
            "let y = &x;"
            "let _ = &mut (*y)"
        "}"),
        "acquire mutable reference");
    REQUIRE(resolve(
        "fn f() { let a = \?\?\?; let b: &I32 = &(*a); b }") ==
        "fn f(): &immut I32 = ({ (let immut a: &immut I32 = (\?\?\?): &immut I32): (); (let immut b: &immut I32 = (&immut (*(a): &immut I32): I32): &immut I32): (); (b): &immut I32 }): &immut I32");
    REQUIRE(resolve(
        "fn f() { let a = \?\?\?; let b: &mut I32 = &mut (*a); b }") ==
        "fn f(): &mut I32 = ({ (let immut a: &mut I32 = (\?\?\?): &mut I32): (); (let immut b: &mut I32 = (&mut (*(a): &mut I32): I32): &mut I32): (); (b): &mut I32 }): &mut I32");
    REQUIRE(resolve(
        "fn f() { let a = \?\?\?; let b = &mut *a; let b: Char = *b; b }") ==
        "fn f(): Char = ({ (let immut a: &mut Char = (\?\?\?): &mut Char): (); (let immut b: &mut Char = (&mut (*(a): &mut Char): Char): &mut Char): (); (let immut b: Char = (*(b): &mut Char): Char): (); (b): Char }): Char");
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
            "(let immut x: String = (\?\?\?): String): (); "
            "(let immut f: fn(String): I64 = (\?\?\?): fn(String): I64): (); "
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
                "Option[String]::some(immut x) -> (x): String; "
                "Option[String]::none -> (\"hello\"): String; "
            "}): String"
        " }): String");
}

TEST("abbreviated constructor pattern") {
    REQUIRE(resolve(
        "enum Option[T] = none | some(T) "
        "fn f(a: Option[I32]) = match a { "
            "::some(b) -> b "
            "_ -> \?\?\?"
        "}")
        ==
        "fn f(immut a: Option[I32]): I32 = ({ "
            "(match (a): Option[I32] { "
                "Option[I32]::some(immut b) -> (b): I32; "
                "_ -> (\?\?\?): I32; "
            "}): I32"
        " }): I32");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Option[T] = none | some(T) "
        "fn f(a: Option[I32]) = match a { ::wasd(x) -> x }"),
        "Option[I32] does not have a constructor 'wasd'");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Option[T] = none | some(T) "
        "fn f() = match \"\" { ::wasd(x) -> x }"),
        "abbreviated constructor pattern used with non-enum type String");
    REQUIRE_RESOLUTION_FAILURE(resolve(
        "enum Option[T] = none | some(T) "
        "fn f() = match \?\?\? { ::wasd(x) -> x }"),
        "abbreviated constructor pattern used with an unsolved unification type variable");
}

TEST("pointer unification") {
    REQUIRE(resolve(
        "fn f(): Char { let x = \?\?\?; unsafe { dereference(addressof(x)) } }")
        ==
        "fn f(): Char = ({ "
            "(let immut x: Char = (\?\?\?): Char): (); "
            "({ (dereference((addressof((x): Char)): *immut Char)): Char }): Char"
        " }): Char");
}

TEST("reference mutability coercion") {
    REQUIRE(resolve("fn f() { let mut x: U8 = 5; let _: &mut U8 = &mut x; }") ==
        "fn f(): () = ({ (let mut x: U8 = (5): U8): (); (let _: &mut U8 = (&mut (x): U8): &mut U8): (); (()): () }): ()");
    REQUIRE(resolve("fn f() { let mut x: U8 = 5; let _: &U8 = &mut x; }") ==
        "fn f(): () = ({ (let mut x: U8 = (5): U8): (); (let _: &immut U8 = (&mut (x): U8): &mut U8): (); (()): () }): ()");
}

TEST("double variable solution") {
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = 5; let _: (I32, I64) = (x, x); }"),        "the explicitly specified type is");
    REQUIRE_RESOLUTION_FAILURE(resolve("fn f() { let x = \?\?\?; let _: (String, I8) = (x, x); }"), "the explicitly specified type is");
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
    REQUIRE(resolve("enum Option[T] = none | some(T) fn f() { let _: Option[I32] = Option[I32]::some(5: I32) }")
         == resolve("enum Option[T] = none | some(T) fn f() { let _ = Option::some(5: I32) }"));
}

TEST("multiple template instantiations") {
    REQUIRE(resolve(
        "enum Option[T] = none | some(T) "
        "fn get[T](_: Option[T]): T = \?\?\?"
        "fn f(): String { let o = \?\?\?; get(o) }"
        "fn g(): I64 { let o = \?\?\?; get(o) }")
        ==
        "fn f(): String = ({ "
            "(let immut o: Option[String] = (\?\?\?): Option[String]): (); "
            "(get[String]((o): Option[String])): String"
        " }): String"
        "fn g(): I64 = ({ "
            "(let immut o: Option[I64] = (\?\?\?): Option[I64]): (); "
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
        "fn f(immut s: S): &immut Char = ({ "
            "({ (let _: Char = (b((&immut (s): S): &immut S)): Char): (); (()): () }): (); "
            "(a[immut]((&immut (s): S): &immut S)): &immut Char"
        " }): &immut Char");
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
        "fn f(immut o: Option[I32]): String = ({ "
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
