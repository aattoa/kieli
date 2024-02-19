#include <libutl/common/utilities.hpp>
#include <libutl/common/pooled_string.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libutl-common pooled-string: " name)

static constexpr auto voidify = [](auto const* const ptr) { return static_cast<void const*>(ptr); };

TEST("equality")
{
    utl::String_pool pool;

    utl::Pooled_string const a = pool.make("abc");
    utl::Pooled_string const b = pool.make("def");
    utl::Pooled_string const c = pool.make("abc");
    utl::Pooled_string const d = pool.make("def");

    REQUIRE_EQUAL(a, c);
    REQUIRE_EQUAL(b, d);

    CHECK_EQUAL(voidify(a.view().data()), voidify(c.view().data()));
    CHECK_EQUAL(voidify(b.view().data()), voidify(d.view().data()));
}

TEST("overlap")
{
    utl::String_pool pool;

    utl::Pooled_string const a = pool.make("ab");
    utl::Pooled_string const b = pool.make("cd");
    utl::Pooled_string const c = pool.make("bc");

    REQUIRE_EQUAL(a.size(), 2UZ);
    REQUIRE_EQUAL(a.view(), "ab");
    REQUIRE_EQUAL(b.size(), 2UZ);
    REQUIRE_EQUAL(b.view(), "cd");
    REQUIRE_EQUAL(c.size(), 2UZ);
    REQUIRE_EQUAL(c.view(), "bc");

    CHECK_EQUAL(voidify(a.view().data() + 1), voidify(c.view().data()));
    CHECK_EQUAL(voidify(b.view().data() - 1), voidify(c.view().data()));

    CHECK_EQUAL("abcd", std::string_view { a.view().data(), b.view().data() + b.size() });
}
