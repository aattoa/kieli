#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include <libcompiler/filesystem.hpp>

#define TEST(name) UNITTEST("libcompiler: filesystem: " name)

TEST("text_range")
{
    // section: one line
    {
        CHECK_EQUAL(kieli::text_range("hello", { { 0, 0 }, { 0, 0 } }), "");
        CHECK_EQUAL(kieli::text_range("hello", { { 0, 5 }, { 0, 5 } }), "");
        CHECK_EQUAL(kieli::text_range("hello", { { 0, 0 }, { 0, 1 } }), "h");
        CHECK_EQUAL(kieli::text_range("hello", { { 0, 2 }, { 0, 4 } }), "ll");
        CHECK_EQUAL(kieli::text_range("hello", { { 0, 0 }, { 0, 5 } }), "hello");
    }
    // section: multiple lines
    {
        std::string_view const string = "abc\ndefg\nhij";
        CHECK_EQUAL(kieli::text_range(string, { { 0, 0 }, { 0, 3 } }), "abc");
        CHECK_EQUAL(kieli::text_range(string, { { 1, 0 }, { 1, 4 } }), "defg");
        CHECK_EQUAL(kieli::text_range(string, { { 2, 0 }, { 2, 3 } }), "hij");
        CHECK_EQUAL(kieli::text_range(string, { { 0, 0 }, { 2, 3 } }), string);
        CHECK_EQUAL(kieli::text_range(string, { { 0, 0 }, { 1, 3 } }), "abc\ndef");
        CHECK_EQUAL(kieli::text_range(string, { { 1, 2 }, { 2, 1 } }), "fg\nh");
    }
}

TEST("edit_text")
{
    std::string text = "lo";

    kieli::edit_text(text, { { 0, 0 }, { 0, 0 } }, "hel");
    REQUIRE_EQUAL(text, "hello");

    kieli::edit_text(text, { { 0, 5 }, { 0, 5 } }, ", world");
    REQUIRE_EQUAL(text, "hello, world");

    kieli::edit_text(text, { { 0, 5 }, { 0, 7 } }, "");
    REQUIRE_EQUAL(text, "helloworld");
}

TEST("advance_position")
{
    kieli::Position position;
    kieli::advance_position(position, 'a');
    REQUIRE_EQUAL(position, kieli::Position { 0, 1 });
    kieli::advance_position(position, '\n');
    REQUIRE_EQUAL(position, kieli::Position { 1, 0 });
}

TEST("find_source")
{
    kieli::Source_vector vector;

    auto const a = vector.push("content A", "path A");
    auto const b = vector.push("content B", "path B");
    auto const c = vector.push("content C", "path C");

    CHECK(kieli::find_source("path A", vector) == a);
    CHECK(kieli::find_source("path B", vector) == b);
    CHECK(kieli::find_source("path C", vector) == c);
}
