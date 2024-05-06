#include <libutl/utilities.hpp>
#include <libutl/safe_integer.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libutl: " name)

namespace {
    template <class T>
    constexpr auto min = std::numeric_limits<T>::min();
    template <class T>
    constexpr auto max = std::numeric_limits<T>::max();
    template <class T>
    constexpr auto safe_min = utl::Safe_integer<T> { min<T> };
    template <class T>
    constexpr auto safe_max = utl::Safe_integer<T> { max<T> };
} // namespace

TEST("safe integer default construction")
{
    CHECK_EQUAL(utl::Safe_integer<int> {}, 0);
}

TEST("safe integer conversion to bool")
{
    CHECK(safe_max<std::int32_t>);
    CHECK(safe_min<std::int32_t>);
    CHECK(utl::Safe_i32 { 1 });
    CHECK(!utl::Safe_i32 { 0 });
}

TEST("safe integer addition")
{
    // SECTION("overflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_overflow, safe_max<int> + 1);
    }
    // SECTION("underflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_underflow, safe_min<int> + -1);
    }
}

TEST("safe integer subtraction")
{
    // SECTION("overflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_overflow, safe_max<int> - -1);
    }
    // SECTION("underflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_underflow, safe_min<int> - 1);
    }
}

TEST("safe integer multiplication")
{
    // SECTION("overflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_overflow, utl::Safe_u8 { 130 } * 2);
    }
    // SECTION("underflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_underflow, utl::Safe_i8 { 50 } * -3);
    }
}

TEST("safe integer division")
{
    // SECTION("division by zero")
    {
        CHECK_THROWS_AS(utl::Safe_integer_division_by_zero, utl::Safe_i64 { 50 } / 0);
    }
    // SECTION("overflow")
    {
        CHECK_THROWS_AS(utl::Safe_integer_overflow, safe_min<int> / -1);
    }
}
