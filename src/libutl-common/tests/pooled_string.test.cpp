#include <libutl/common/utilities.hpp>
#include <libutl/common/pooled_string.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("pooled-string " name, "[libutl][pooled_string]")


TEST("equality") {
    utl::String_pool pool { /*initial_capacity=*/ 32 };

    utl::Pooled_string const a = pool.make("abc");
    utl::Pooled_string const b = pool.make("def");
    utl::Pooled_string const c = pool.make("abc");
    utl::Pooled_string const d = pool.make("def");

    REQUIRE(a == c);
    REQUIRE(b == d);
}


TEST("overlap") {
    utl::String_pool pool { /*initial_capacity=*/ 32 };

    utl::Pooled_string const a = pool.make("ab");
    utl::Pooled_string const b = pool.make("cd");
    utl::Pooled_string const c = pool.make("bc");

    REQUIRE(a.size() == 2); REQUIRE(a == "ab");
    REQUIRE(b.size() == 2); REQUIRE(b == "cd");
    REQUIRE(c.size() == 2); REQUIRE(c == "bc");

    REQUIRE(a.view().data() + 1 == c.view().data());
    REQUIRE(b.view().data() - 1 == c.view().data());

    REQUIRE("abcd" == std::string_view { a.view().data(), b.view().data() + b.size() });
}
