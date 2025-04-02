#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include <libcompiler/compiler.hpp>

#define TEST(name) UNITTEST("libcompiler: document: " name)

namespace {
    auto range(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) -> ki::Range
    {
        return ki::Range({ .line = a, .column = b }, { .line = c, .column = d });
    }
} // namespace

TEST("text_range")
{
    // section: one line
    {
        CHECK_EQUAL(ki::text_range("hello", range(0, 0, 0, 0)), "");
        CHECK_EQUAL(ki::text_range("hello", range(0, 5, 0, 5)), "");
        CHECK_EQUAL(ki::text_range("hello", range(0, 0, 0, 1)), "h");
        CHECK_EQUAL(ki::text_range("hello", range(0, 2, 0, 4)), "ll");
        CHECK_EQUAL(ki::text_range("hello", range(0, 0, 0, 5)), "hello");
    }
    // section: multiple lines
    {
        std::string_view const string = "abc\ndefg\nhij";
        CHECK_EQUAL(ki::text_range(string, range(0, 0, 0, 3)), "abc");
        CHECK_EQUAL(ki::text_range(string, range(1, 0, 1, 4)), "defg");
        CHECK_EQUAL(ki::text_range(string, range(2, 0, 2, 3)), "hij");
        CHECK_EQUAL(ki::text_range(string, range(0, 0, 2, 3)), string);
        CHECK_EQUAL(ki::text_range(string, range(0, 0, 1, 3)), "abc\ndef");
        CHECK_EQUAL(ki::text_range(string, range(1, 2, 2, 1)), "fg\nh");
    }
}

TEST("edit_text")
{
    std::string text = "lo";

    ki::edit_text(text, range(0, 0, 0, 0), "hel");
    REQUIRE_EQUAL(text, "hello");

    ki::edit_text(text, range(0, 5, 0, 5), ", world");
    REQUIRE_EQUAL(text, "hello, world");

    ki::edit_text(text, range(0, 5, 0, 7), "");
    REQUIRE_EQUAL(text, "helloworld");
}

TEST("Position::advance_with")
{
    ki::Position position;
    position.advance_with('a');
    REQUIRE_EQUAL(position, ki::Position { 0, 1 });
    position.advance_with('\n');
    REQUIRE_EQUAL(position, ki::Position { 1, 0 });
}
