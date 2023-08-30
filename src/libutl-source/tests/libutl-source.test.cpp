#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE(name, "[libutl][source]")

TEST("utl::Source::Source")
{
    utl::Source const source { "filename 123", "content 321" };
    REQUIRE(source.path().string() == "filename 123");
    REQUIRE(source.string() == "content 321");
}

TEST("utl::Source::read")
{
    auto path = std::filesystem::temp_directory_path() / "temp-kieli-libutl-source-read-test.txt";
    static constexpr std::string_view test_string = "test string 123456789";

    {
        REQUIRE(!std::filesystem::exists(path));
        std::ofstream file { path };
        REQUIRE(file.is_open());
        file << test_string;
    }
    [[maybe_unused]] auto const file_remover
        = utl::on_scope_exit([path] { std::filesystem::remove(path); });

    std::string const path_string = path.string();
    utl::Source const source      = utl::Source::read(std::move(path));
    REQUIRE(source.path().string() == path_string);
    REQUIRE(source.string() == test_string);
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

TEST("utl::Source_view::combine_with")
{
    auto                    source_arena = utl::Source::Arena::with_page_size(1);
    utl::wrapper auto const source       = source_arena.wrap("test source", "Hello, world!");
    REQUIRE(source->string() == "Hello, world!");

    utl::Source_view const view_1 { source, source->string().substr(0, 5), {}, {} };
    utl::Source_view const view_2 { source, source->string().substr(7, 5), {}, {} };
    REQUIRE(view_1.string == "Hello");
    REQUIRE(view_2.string == "world");

    utl::Source_view const combined_view = view_1.combine_with(view_2);
    REQUIRE(combined_view.string == "Hello, world");
    REQUIRE(combined_view.string.data() == view_1.string.data());
}

TEST("utl::Source_view::dummy")
{
    REQUIRE(utl::Source_view::dummy().source.is(utl::Source_view::dummy().source));
}

static_assert(!std::is_default_constructible_v<utl::Source>);
static_assert(!std::is_default_constructible_v<utl::Source_view>);
static_assert(std::is_trivially_copyable_v<utl::Source_view>);
static_assert(std::is_trivially_copyable_v<utl::Source_position>);
static_assert(utl::Source_position { 4, 5 } < utl::Source_position { 9, 2 });
static_assert(utl::Source_position { 5, 2 } < utl::Source_position { 5, 3 });
static_assert(utl::Source_position { 3, 2 } > utl::Source_position { 2, 3 });
