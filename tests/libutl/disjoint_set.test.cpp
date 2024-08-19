#include <libutl/utilities.hpp>
#include <libutl/disjoint_set.hpp>
#include <cppunittest/unittest.hpp>

namespace {
    auto in_same_set(utl::Disjoint_set& set, std::size_t const x, std::size_t const y) -> bool
    {
        bool const equal = set.find_without_compressing(x) == set.find_without_compressing(y);
        REQUIRE_EQUAL(equal, set.find_without_compressing(y) == set.find_without_compressing(x));
        REQUIRE_EQUAL(equal, set.find(x) == set.find(y));
        REQUIRE_EQUAL(equal, set.find(y) == set.find(x));
        return equal;
    }

    auto has_no_parent(utl::Disjoint_set& set, std::size_t const x) -> bool
    {
        bool const equal = x == set.find_without_compressing(x);
        REQUIRE_EQUAL(equal, x == set.find(x));
        return equal;
    }
} // namespace

UNITTEST("libutl disjoint_set")
{
    utl::Disjoint_set set { 10 };

    for (std::size_t i = 0; i != 10; ++i) {
        CHECK(has_no_parent(set, i));
    }

    set.merge(0, 2);
    set.merge(2, 4);
    set.merge(7, 9);

    CHECK(in_same_set(set, 0, 2));
    CHECK(in_same_set(set, 2, 4));
    CHECK(in_same_set(set, 0, 4));
    CHECK(in_same_set(set, 7, 9));

    CHECK(has_no_parent(set, 1));
    CHECK(has_no_parent(set, 3));
    CHECK(has_no_parent(set, 5));
    CHECK(has_no_parent(set, 6));
    CHECK(has_no_parent(set, 8));

    CHECK_THROWS_AS(std::out_of_range, set.find(10));
}
