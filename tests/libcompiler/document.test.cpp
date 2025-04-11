#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include <libcompiler/db.hpp>

using namespace ki::db;
using namespace ki::lsp;

namespace {
    auto range(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) -> Range
    {
        return Range({ .line = a, .column = b }, { .line = c, .column = d });
    }
} // namespace

UNITTEST("ki::db::text_range")
{
    // section: one line
    {
        CHECK_EQUAL(text_range("hello", range(0, 0, 0, 0)), "");
        CHECK_EQUAL(text_range("hello", range(0, 5, 0, 5)), "");
        CHECK_EQUAL(text_range("hello", range(0, 0, 0, 1)), "h");
        CHECK_EQUAL(text_range("hello", range(0, 2, 0, 4)), "ll");
        CHECK_EQUAL(text_range("hello", range(0, 0, 0, 5)), "hello");
    }
    // section: multiple lines
    {
        std::string_view const string = "abc\ndefg\nhij";
        CHECK_EQUAL(text_range(string, range(0, 0, 0, 3)), "abc");
        CHECK_EQUAL(text_range(string, range(1, 0, 1, 4)), "defg");
        CHECK_EQUAL(text_range(string, range(2, 0, 2, 3)), "hij");
        CHECK_EQUAL(text_range(string, range(0, 0, 2, 3)), string);
        CHECK_EQUAL(text_range(string, range(0, 0, 1, 3)), "abc\ndef");
        CHECK_EQUAL(text_range(string, range(1, 2, 2, 1)), "fg\nh");
    }
}

UNITTEST("ki::db::edit_text")
{
    std::string text = "lo";

    edit_text(text, range(0, 0, 0, 0), "hel");
    REQUIRE_EQUAL(text, "hello");

    edit_text(text, range(0, 5, 0, 5), ", world");
    REQUIRE_EQUAL(text, "hello, world");

    edit_text(text, range(0, 5, 0, 7), "");
    REQUIRE_EQUAL(text, "helloworld");
}

UNITTEST("ki::db::advance")
{
    Position position;

    position = advance(position, 'a');
    REQUIRE_EQUAL(position, Position { 0, 1 });

    position = advance(position, '\n');
    REQUIRE_EQUAL(position, Position { 1, 0 });
}
