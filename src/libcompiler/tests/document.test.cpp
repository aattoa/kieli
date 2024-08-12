#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include <libcompiler/compiler.hpp>

#define TEST(name) UNITTEST("libcompiler: document: " name)

TEST("text_range")
{
    // section: one line
    {
        CHECK_EQUAL(kieli::text_range("hello", kieli::Range({ 0, 0 }, { 0, 0 })), "");
        CHECK_EQUAL(kieli::text_range("hello", kieli::Range({ 0, 5 }, { 0, 5 })), "");
        CHECK_EQUAL(kieli::text_range("hello", kieli::Range({ 0, 0 }, { 0, 1 })), "h");
        CHECK_EQUAL(kieli::text_range("hello", kieli::Range({ 0, 2 }, { 0, 4 })), "ll");
        CHECK_EQUAL(kieli::text_range("hello", kieli::Range({ 0, 0 }, { 0, 5 })), "hello");
    }
    // section: multiple lines
    {
        std::string_view const string = "abc\ndefg\nhij";
        CHECK_EQUAL(kieli::text_range(string, kieli::Range({ 0, 0 }, { 0, 3 })), "abc");
        CHECK_EQUAL(kieli::text_range(string, kieli::Range({ 1, 0 }, { 1, 4 })), "defg");
        CHECK_EQUAL(kieli::text_range(string, kieli::Range({ 2, 0 }, { 2, 3 })), "hij");
        CHECK_EQUAL(kieli::text_range(string, kieli::Range({ 0, 0 }, { 2, 3 })), string);
        CHECK_EQUAL(kieli::text_range(string, kieli::Range({ 0, 0 }, { 1, 3 })), "abc\ndef");
        CHECK_EQUAL(kieli::text_range(string, kieli::Range({ 1, 2 }, { 2, 1 })), "fg\nh");
    }
}

TEST("edit_text")
{
    std::string text = "lo";

    kieli::edit_text(text, kieli::Range({ 0, 0 }, { 0, 0 }), "hel");
    REQUIRE_EQUAL(text, "hello");

    kieli::edit_text(text, kieli::Range({ 0, 5 }, { 0, 5 }), ", world");
    REQUIRE_EQUAL(text, "hello, world");

    kieli::edit_text(text, kieli::Range({ 0, 5 }, { 0, 7 }), "");
    REQUIRE_EQUAL(text, "helloworld");
}

TEST("Position::advance_with")
{
    kieli::Position position;
    position.advance_with('a');
    REQUIRE_EQUAL(position, kieli::Position { 0, 1 });
    position.advance_with('\n');
    REQUIRE_EQUAL(position, kieli::Position { 1, 0 });
}

TEST("find_existing_document_id")
{
    kieli::Database db;

    auto const a = kieli::client_open_document(db, "path A", "content A");
    auto const b = kieli::client_open_document(db, "path B", "content B");
    auto const c = kieli::client_open_document(db, "path C", "content C");

    CHECK(kieli::find_existing_document_id(db, "path A") == a);
    CHECK(kieli::find_existing_document_id(db, "path B") == b);
    CHECK(kieli::find_existing_document_id(db, "path C") == c);

    CHECK_EQUAL(db.documents.at(a).text, "content A");
    CHECK_EQUAL(db.documents.at(b).text, "content B");
    CHECK_EQUAL(db.documents.at(c).text, "content C");
}
