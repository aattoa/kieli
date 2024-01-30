#include <libutl/common/utilities.hpp>
#include <libutl/common/pooled_string.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libutl-common pooled-string: " name)

TEST("equality")
{
    utl::String_pool pool { /*initial_capacity=*/32 };

    utl::Pooled_string const a = pool.make("abc");
    utl::Pooled_string const b = pool.make("def");
    utl::Pooled_string const c = pool.make("abc");
    utl::Pooled_string const d = pool.make("def");

    CHECK_EQUAL(a, c);
    CHECK_EQUAL(b, d);
}

TEST("overlap")
{
    utl::String_pool pool { /*initial_capacity=*/32 };

    utl::Pooled_string const a = pool.make("ab");
    utl::Pooled_string const b = pool.make("cd");
    utl::Pooled_string const c = pool.make("bc");

    CHECK_EQUAL(a.size(), 2UZ);
    CHECK_EQUAL(a.view(), "ab");

    CHECK_EQUAL(b.size(), 2UZ);
    CHECK_EQUAL(b.view(), "cd");

    CHECK_EQUAL(c.size(), 2UZ);
    CHECK_EQUAL(c.view(), "bc");

    CHECK_EQUAL(a.view().data() + 1, c.view().data());
    CHECK_EQUAL(b.view().data() - 1, c.view().data());

    CHECK_EQUAL("abcd", std::string_view { a.view().data(), b.view().data() + b.size() });
}
