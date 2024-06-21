#include <libutl/utilities.hpp>
#include <libutl/source.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libutl: " name)

TEST("Source_position::advance_with")
{
    utl::Source_position position { .line = 5, .column = 7 };
    position.advance_with('a');
    CHECK_EQUAL(position, utl::Source_position { .line = 5, .column = 8 });
    position.advance_with('\t');
    CHECK_EQUAL(position, utl::Source_position { .line = 5, .column = 9 });
    position.advance_with('\n');
    CHECK_EQUAL(position, utl::Source_position { .line = 6, .column = 1 });
    position.advance_with('b');
    CHECK_EQUAL(position, utl::Source_position { .line = 6, .column = 2 });
}

TEST("Source_range::in")
{
    static constexpr std::string_view source = "123abc\n"
                                               "456defg\n"
                                               "789hij";
    CHECK_EQUAL(utl::Source_range { { 1, 1 }, { 1, 3 } }.in(source), "123");
    CHECK_EQUAL(utl::Source_range { { 2, 4 }, { 2, 6 } }.in(source), "def");
    CHECK_EQUAL(utl::Source_range { { 3, 1 }, { 3, 6 } }.in(source), "789hij");
    CHECK_EQUAL(utl::Source_range { { 3, 2 }, { 3, 5 } }.in(source), "89hi");
    CHECK_EQUAL(utl::Source_range { { 1, 2 }, { 2, 5 } }.in(source), "23abc\n456de");
    CHECK_EQUAL(utl::Source_range { { 2, 1 }, { 3, 1 } }.in(source), "456defg\n7");
    CHECK_EQUAL(utl::Source_range { { 1, 1 }, { 3, 6 } }.in(source), source);
}

static_assert(!std::is_default_constructible_v<utl::Source_range>);
static_assert(std::is_trivially_copyable_v<utl::Source_range>);
static_assert(std::is_trivially_copyable_v<utl::Source_position>);
static_assert(utl::Source_position { 4, 5 } < utl::Source_position { 9, 2 });
static_assert(utl::Source_position { 5, 2 } < utl::Source_position { 5, 3 });
static_assert(utl::Source_position { 3, 2 } > utl::Source_position { 2, 3 });
