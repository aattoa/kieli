#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libutl: " name)

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

template <>
struct std::formatter<Move_only> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(Move_only const& move_only, auto& context)
    {
        return std::format_to(context.out(), "Move_only({})", move_only.value);
    }
};

TEST("to_vector")
{
    std::vector<Move_only> vector;
    vector.push_back(10_mov);
    vector.push_back(20_mov);
    vector.push_back(30_mov);
    CHECK_EQUAL(vector, utl::to_vector({ 10_mov, 20_mov, 30_mov }));
}

TEST("pop_back")
{
    auto vector = utl::to_vector({ 10_mov, 20_mov, 30_mov });
    CHECK_EQUAL(utl::pop_back(vector).value(), 30_mov);
    CHECK_EQUAL(utl::pop_back(vector).value(), 20_mov);
    CHECK_EQUAL(utl::pop_back(vector).value(), 10_mov);
    CHECK(!utl::pop_back(vector).has_value());
}

static_assert(utl::losslessly_convertible_to<std::int8_t, std::int16_t>);
static_assert(utl::losslessly_convertible_to<std::int32_t, std::int32_t>);
static_assert(utl::losslessly_convertible_to<std::uint8_t, std::int32_t>);
static_assert(utl::losslessly_convertible_to<std::uint32_t, std::int64_t>);
static_assert(!utl::losslessly_convertible_to<std::int8_t, std::uint8_t>);
static_assert(!utl::losslessly_convertible_to<std::uint64_t, std::int8_t>);
static_assert(!utl::losslessly_convertible_to<std::int8_t, std::uint64_t>);
static_assert(!utl::losslessly_convertible_to<std::int16_t, std::int8_t>);
