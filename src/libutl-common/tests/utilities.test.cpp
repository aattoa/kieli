#include <libutl/common/utilities.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("libutl " name, "[libutl][utilities]") // NOLINT

namespace {
    struct Move_only {
        int value;

        explicit constexpr Move_only(int const value) noexcept : value(value) {}

        ~Move_only()                                       = default;
        Move_only(Move_only const&)                        = delete;
        auto operator=(Move_only const&)                   = delete;
        Move_only(Move_only&&) noexcept                    = default;
        auto operator=(Move_only&&) noexcept -> Move_only& = default;

        auto operator==(Move_only const&) const -> bool = default;
    };

    constexpr auto operator""_mov(unsigned long long const value) -> Move_only
    {
        return Move_only { utl::safe_cast<int>(value) };
    }

    static_assert(std::movable<Move_only> && !std::copyable<Move_only>);
} // namespace

TEST("vector capacity operations")
{
    auto vector = utl::vector_with_capacity<int>(10);
    REQUIRE(vector.empty());
    REQUIRE(vector.capacity() >= 10);
    utl::release_vector_memory(vector);
    REQUIRE(vector.empty());
    REQUIRE(vector.capacity() == 0);
}

TEST("to_vector")
{
    std::vector<Move_only> vector;
    vector.push_back(10_mov);
    vector.push_back(20_mov);
    vector.push_back(30_mov);
    REQUIRE(vector == utl::to_vector({ 10_mov, 20_mov, 30_mov }));
}

TEST("resize_down_vector")
{
    auto vector = utl::to_vector({ 20_mov, 40_mov, 60_mov, 80_mov });
    utl::resize_down_vector(vector, 2);
    REQUIRE(vector == utl::to_vector({ 20_mov, 40_mov }));
    REQUIRE(vector.capacity() >= 4);
    utl::resize_down_vector(vector, 0);
    REQUIRE(vector.empty());
    REQUIRE(vector.capacity() >= 4);
}

TEST("append_vector")
{
    SECTION("from rvalue")
    {
        auto vector = utl::to_vector({ 10_mov, 20_mov, 30_mov });
        utl::append_vector(vector, utl::to_vector({ 40_mov, 50_mov, 60_mov }));
        REQUIRE(vector == utl::to_vector({ 10_mov, 20_mov, 30_mov, 40_mov, 50_mov, 60_mov }));
    }
    SECTION("from lvalue")
    {
        auto to   = utl::to_vector({ 100_mov, 200_mov });
        auto from = utl::to_vector({ 300_mov, 400_mov });
        utl::append_vector(to, std::move(from));
        REQUIRE(to == utl::to_vector({ 100_mov, 200_mov, 300_mov, 400_mov }));
        REQUIRE(from.empty()); // NOLINT: use after move
    }
}

TEST("find_nth_if")
{
    static constexpr auto array   = std::to_array({ 1, 2, 3, 4, 5 });
    static constexpr auto is_even = [](auto const x) { return x % 2 == 0; };
    REQUIRE(utl::find_nth_if(array.begin(), array.end(), 0, is_even) == array.begin() + 1);
    REQUIRE(utl::find_nth_if(array.begin(), array.end(), 1, is_even) == array.begin() + 3);
    REQUIRE(utl::find_nth_if(array.begin(), array.end(), 2, is_even) == array.end());
}

TEST("find_nth")
{
    static constexpr std::string_view string = "hello, world!";
    REQUIRE(utl::find_nth(string.begin(), string.end(), 0, 'l') == string.begin() + 2);
    REQUIRE(utl::find_nth(string.begin(), string.end(), 1, 'l') == string.begin() + 3);
    REQUIRE(utl::find_nth(string.begin(), string.end(), 2, 'l') == string.begin() + 10);
    REQUIRE(utl::find_nth(string.begin(), string.end(), 3, 'l') == string.end());
}

TEST("map")
{
    static constexpr auto square
        = [](Move_only const x) { return Move_only { x.value * x.value }; };
    REQUIRE(
        utl::map(square, utl::to_vector({ 1_mov, 2_mov, 3_mov }))
        == utl::to_vector({ 1_mov, 4_mov, 9_mov }));
    REQUIRE(
        utl::map(square)(utl::to_vector({ 1_mov, 2_mov, 3_mov }))
        == utl::to_vector({ 1_mov, 4_mov, 9_mov }));
}

TEST("Relative_string")
{
    SECTION("view_in")
    {
        static constexpr utl::Relative_string rs { .offset = 2, .length = 3 };
        REQUIRE(rs.view_in("abcdefg") == "cde");
    }
    SECTION("format_to")
    {
        std::string s  = "abc";
        auto const  rs = utl::Relative_string::format_to(s, "d{}fg", 'e');
        REQUIRE(s == "abcdefg");
        REQUIRE(rs.offset == 3);
        REQUIRE(rs.length == 4);
        REQUIRE(rs.view_in(s) == "defg");
    }
}

TEST("basename")
{
    REQUIRE(utl::basename("aaa/bbb/ccc") == "ccc");
    REQUIRE(utl::basename("aaa\\bbb\\ccc") == "ccc");
}

static_assert(utl::losslessly_convertible_to<utl::I8, utl::I16>);
static_assert(utl::losslessly_convertible_to<utl::I32, utl::I32>);
static_assert(utl::losslessly_convertible_to<utl::U8, utl::I32>);
static_assert(utl::losslessly_convertible_to<utl::U32, utl::I64>);
static_assert(!utl::losslessly_convertible_to<utl::I8, utl::U8>);
static_assert(!utl::losslessly_convertible_to<utl::U64, utl::I8>);
static_assert(!utl::losslessly_convertible_to<utl::I8, utl::U64>);
static_assert(!utl::losslessly_convertible_to<utl::I16, utl::I8>);

static constexpr auto composition = utl::compose(
    [](int x) { return x * x; }, [](int x) { return x + 1; }, [](int a, int b) { return a + b; });
static_assert(composition(2, 3) == 36);
