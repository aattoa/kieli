#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE(name, "[libutl][safe_integer]") // NOLINT

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
    REQUIRE(utl::Safe_integer<int> {} == 0);
}

TEST("safe integer conversion to bool")
{
    REQUIRE(static_cast<bool>(safe_max<utl::I32>));
    REQUIRE(static_cast<bool>(safe_min<utl::I32>));
    REQUIRE(static_cast<bool>(utl::Safe_i32 { 1 }));
    REQUIRE_FALSE(static_cast<bool>(utl::Safe_i32 { 0 }));
}

TEST("safe integer addition")
{
    SECTION("overflow")
    {
        REQUIRE_THROWS_AS(safe_max<int> + 1, utl::Safe_integer_overflow);
    }
    SECTION("underflow")
    {
        REQUIRE_THROWS_AS(safe_min<int> + -1, utl::Safe_integer_underflow);
    }
}

TEST("safe integer subtraction")
{
    SECTION("overflow")
    {
        REQUIRE_THROWS_AS(safe_max<int> - -1, utl::Safe_integer_overflow);
    }
    SECTION("underflow")
    {
        REQUIRE_THROWS_AS(safe_min<int> - 1, utl::Safe_integer_underflow);
    }
}

TEST("safe integer multiplication")
{
    SECTION("overflow")
    {
        REQUIRE_THROWS_AS(utl::Safe_u8 { 130 } * 2, utl::Safe_integer_overflow);
    }
    SECTION("underflow")
    {
        REQUIRE_THROWS_AS(utl::Safe_i8 { 50 } * -3, utl::Safe_integer_underflow);
    }
}

TEST("safe integer division")
{
    SECTION("division by zero")
    {
        REQUIRE_THROWS_AS(utl::Safe_i64 { 50 } / 0, utl::Safe_integer_division_by_zero);
    }
    SECTION("overflow")
    {
        REQUIRE_THROWS_AS(safe_min<int> / -1, utl::Safe_integer_overflow);
    }
}
