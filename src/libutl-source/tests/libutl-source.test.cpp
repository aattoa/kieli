#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <cpputil/io.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE(name, "[libutl][source]") // NOLINT

TEST("utl::Source::Source")
{
    utl::Source const source { "filename 123", "content 321" };
    REQUIRE(source.path().string() == "filename 123");
    REQUIRE(source.string() == "content 321");
}

TEST("utl::Source_position::advance_with")
{
    utl::Source_position position { .line = 5, .column = 7 };
    position.advance_with('a');
    REQUIRE(position == utl::Source_position { .line = 5, .column = 8 });
    position.advance_with('\t');
    REQUIRE(position == utl::Source_position { .line = 5, .column = 9 });
    position.advance_with('\n');
    REQUIRE(position == utl::Source_position { .line = 6, .column = 1 });
    position.advance_with('b');
    REQUIRE(position == utl::Source_position { .line = 6, .column = 2 });
}

TEST("utl::Source_range::in")
{
    static constexpr std::string_view source = "123abc\n"
                                               "456defg\n"
                                               "789hij";
    REQUIRE(utl::Source_range { { 1, 1 }, { 1, 3 } }.in(source) == "123");
    REQUIRE(utl::Source_range { { 2, 4 }, { 2, 6 } }.in(source) == "def");
    REQUIRE(utl::Source_range { { 3, 1 }, { 3, 6 } }.in(source) == "789hij");
    REQUIRE(utl::Source_range { { 3, 2 }, { 3, 5 } }.in(source) == "89hi");
    REQUIRE(utl::Source_range { { 1, 2 }, { 2, 5 } }.in(source) == "23abc\n456de");
    REQUIRE(utl::Source_range { { 2, 1 }, { 3, 1 } }.in(source) == "456defg\n7");
    REQUIRE(utl::Source_range { { 1, 1 }, { 3, 6 } }.in(source) == source);
}

static_assert(!std::is_default_constructible_v<utl::Source>);
static_assert(!std::is_default_constructible_v<utl::Source_range>);
static_assert(std::is_trivially_copyable_v<utl::Source_range>);
static_assert(std::is_trivially_copyable_v<utl::Source_position>);
static_assert(utl::Source_position { 4, 5 } < utl::Source_position { 9, 2 });
static_assert(utl::Source_position { 5, 2 } < utl::Source_position { 5, 3 });
static_assert(utl::Source_position { 3, 2 } > utl::Source_position { 2, 3 });
