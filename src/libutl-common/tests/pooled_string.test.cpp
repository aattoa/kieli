#include <libutl/common/utilities.hpp>
#include <libutl/common/pooled_string.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE(name, "[libutl][pooled_string]")

namespace {
    using String = utl::Pooled_string<struct Test_pooled_string_tag>;
}


TEST("string pool") {
    String::Pool pool { /*initial_capacity=*/ 32 };

    String const a = pool.make("ab");
    String const b = pool.make("cd");
    String const c = pool.make("bc");

    REQUIRE(a.size() == 2); REQUIRE(a == "ab");
    REQUIRE(b.size() == 2); REQUIRE(b == "cd");
    REQUIRE(c.size() == 2); REQUIRE(c == "bc");

    REQUIRE(a.view().data() + 1 == c.view().data());
    REQUIRE(b.view().data() - 1 == c.view().data());

    REQUIRE("abcd" == std::string_view { a.view().data(), b.view().data() + b.size() });
}
