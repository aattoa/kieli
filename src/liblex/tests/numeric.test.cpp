#include <libutl/common/utilities.hpp>
#include <liblex/numeric.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("liblex: numeric: " name)

TEST("apply_scientific_exponent")
{
    static constexpr auto max_usize = std::numeric_limits<std::size_t>::max();
    CHECK_EQUAL(liblex::apply_scientific_exponent(35, 0), 35);
    CHECK_EQUAL(liblex::apply_scientific_exponent(35, 1), 350);
    CHECK_EQUAL(liblex::apply_scientific_exponent(35, 2), 3500);
    CHECK_EQUAL(liblex::apply_scientific_exponent(max_usize, 0), max_usize);
    CHECK_EQUAL(
        liblex::apply_scientific_exponent(max_usize, 1),
        std::unexpected { liblex::Numeric_error::out_of_range });
}

TEST("parse_integer")
{
    CHECK_EQUAL(liblex::parse_integer("100", 10), 100);
    CHECK_EQUAL(liblex::parse_integer("100", 15), 225);
    CHECK_EQUAL(liblex::parse_integer("100", 20), 400);
    CHECK_EQUAL(
        liblex::parse_integer("3", 2), std::unexpected { liblex::Numeric_error::invalid_argument });
    CHECK_EQUAL(
        liblex::parse_integer("9999999999999999999999999999"),
        std::unexpected { liblex::Numeric_error::out_of_range });
    CHECK_EQUAL(
        liblex::parse_integer("5w"), std::unexpected { liblex::Numeric_error::invalid_argument });
    CHECK_EQUAL(
        liblex::parse_integer("w5"), std::unexpected { liblex::Numeric_error::invalid_argument });
}

TEST("parse_floating")
{
    CHECK_EQUAL(liblex::parse_floating("3.14"), 3.14);
    CHECK_EQUAL(liblex::parse_floating("3.14e0"), 3.14);
    CHECK_EQUAL(liblex::parse_floating("3.14e1"), 31.4);
    CHECK_EQUAL(liblex::parse_floating("3.14e2"), 314.);
    CHECK_EQUAL(
        liblex::parse_floating("3.14e9999999999999999999999999999"),
        std::unexpected { liblex::Numeric_error::out_of_range });
}
