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
    std::vector<int> vector;
    REQUIRE(vector.empty());
    vector.reserve(10);
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

TEST("pop_back")
{
    auto vector = utl::to_vector({ 10_mov, 20_mov, 30_mov });
    REQUIRE(utl::pop_back(vector) == 30_mov);
    REQUIRE(utl::pop_back(vector) == 20_mov);
    REQUIRE(utl::pop_back(vector) == 10_mov);
    REQUIRE(utl::pop_back(vector) == std::nullopt);
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

static_assert(utl::losslessly_convertible_to<std::int8_t, std::int16_t>);
static_assert(utl::losslessly_convertible_to<std::int32_t, std::int32_t>);
static_assert(utl::losslessly_convertible_to<std::uint8_t, std::int32_t>);
static_assert(utl::losslessly_convertible_to<std::uint32_t, std::int64_t>);
static_assert(!utl::losslessly_convertible_to<std::int8_t, std::uint8_t>);
static_assert(!utl::losslessly_convertible_to<std::uint64_t, std::int8_t>);
static_assert(!utl::losslessly_convertible_to<std::int8_t, std::uint64_t>);
static_assert(!utl::losslessly_convertible_to<std::int16_t, std::int8_t>);
