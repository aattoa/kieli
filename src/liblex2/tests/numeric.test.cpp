#include <libutl/common/utilities.hpp>
#include <liblex2/numeric.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE(name, "[liblex][numeric]") // NOLINT

TEST("liblex::apply_scientific_exponent")
{
    static constexpr auto max_usize = std::numeric_limits<std::size_t>::max();
    REQUIRE(liblex2::apply_scientific_exponent(35, 0) == 35);
    REQUIRE(liblex2::apply_scientific_exponent(35, 1) == 350);
    REQUIRE(liblex2::apply_scientific_exponent(35, 2) == 3500);
    REQUIRE(liblex2::apply_scientific_exponent(max_usize, 0) == max_usize);
    REQUIRE(
        liblex2::apply_scientific_exponent(max_usize, 1)
        == std::unexpected { liblex2::Numeric_error::out_of_range });
}

TEST("liblex::parse_integer")
{
    REQUIRE(liblex2::parse_integer("100", 10) == 100);
    REQUIRE(liblex2::parse_integer("100", 15) == 225);
    REQUIRE(liblex2::parse_integer("100", 20) == 400);
    REQUIRE(
        liblex2::parse_integer("3", 2)
        == std::unexpected { liblex2::Numeric_error::invalid_argument });
    REQUIRE(
        liblex2::parse_integer("9999999999999999999999999999")
        == std::unexpected { liblex2::Numeric_error::out_of_range });
    REQUIRE(
        liblex2::parse_integer("5w")
        == std::unexpected { liblex2::Numeric_error::invalid_argument });
    REQUIRE(
        liblex2::parse_integer("w5")
        == std::unexpected { liblex2::Numeric_error::invalid_argument });
}

TEST("liblex2::parse_floating")
{
    REQUIRE(liblex2::parse_floating("3.14") == 3.14);
    REQUIRE(liblex2::parse_floating("3.14e0") == 3.14);
    REQUIRE(liblex2::parse_floating("3.14e1") == 31.4);
    REQUIRE(liblex2::parse_floating("3.14e2") == 314.);
    REQUIRE(
        liblex2::parse_floating("3.14e9999999999999999999999999999")
        == std::unexpected { liblex2::Numeric_error::out_of_range });
}
