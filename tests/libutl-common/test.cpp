#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <libutl/common/flatmap.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE(name, "[utl]")


TEST("vector") {
    auto vector = utl::vector_with_capacity<int>(10);
    REQUIRE(vector.size() == 0_uz);
    REQUIRE(vector.capacity() == 10_uz);

    utl::release_vector_memory(vector);
    REQUIRE(vector.size() == 0_uz);
    REQUIRE(vector.capacity() == 0_uz);
}


TEST("safe integer") {
    utl::Safe_integer<int> integer;
    REQUIRE(integer == 0);
    REQUIRE_FALSE(static_cast<bool>(integer));

    integer += 5;
    REQUIRE(integer == 5);
    REQUIRE(static_cast<bool>(integer));

    (void)(integer + 5);
    REQUIRE(integer == 5);

    REQUIRE_THROWS_AS((void)(integer / 0), utl::Safe_integer_division_by_zero);

    integer = std::numeric_limits<int>::max();
    REQUIRE_THROWS_AS((void)(++integer), utl::Safe_integer_overflow);

    integer = std::numeric_limits<int>::min();
    REQUIRE_THROWS_AS((void)(--integer), utl::Safe_integer_underflow);
}


TEST("flatmap") {
    utl::Flatmap<int, int> flatmap;

    flatmap.add_or_assign(10, 20);
    REQUIRE(flatmap.size() == 1_uz);
    REQUIRE(flatmap.find(10) != nullptr);
    REQUIRE(*flatmap.find(10) == 20);

    flatmap.add_or_assign(10, 30);
    REQUIRE(flatmap.size() == 1_uz);
    REQUIRE(flatmap.find(10) != nullptr);
    REQUIRE(*flatmap.find(10) == 30);

    flatmap.add_or_assign(20, 40);
    REQUIRE(flatmap.size() == 2_uz);
    REQUIRE(flatmap.find(20) != nullptr);
    REQUIRE(*flatmap.find(20) == 40);
}
