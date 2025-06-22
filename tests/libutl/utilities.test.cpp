#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>

using namespace ki;

namespace {
    struct Move_only {
        int value;

        explicit constexpr Move_only(int value) noexcept : value(value) {}

        ~Move_only()                                       = default;
        Move_only(Move_only const&)                        = delete;
        auto operator=(Move_only const&)                   = delete;
        Move_only(Move_only&&) noexcept                    = default;
        auto operator=(Move_only&&) noexcept -> Move_only& = default;

        auto operator==(Move_only const&) const -> bool = default;
    };

    consteval auto operator""_mov(unsigned long long value) -> Move_only
    {
        return Move_only(static_cast<int>(value));
    }

    static_assert(std::movable<Move_only> and not std::copyable<Move_only>);
} // namespace

template <>
struct std::formatter<Move_only> {
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(Move_only const& move_only, auto& ctx)
    {
        return std::format_to(ctx.out(), "Move_only({})", move_only.value);
    }
};

UNITTEST("utl::to_vector")
{
    std::vector<Move_only> vector;
    vector.push_back(10_mov);
    vector.push_back(20_mov);
    vector.push_back(30_mov);
    CHECK_EQUAL(vector, utl::to_vector({ 10_mov, 20_mov, 30_mov }));
}

UNITTEST("utl::View")
{
    std::string_view string = "Hello, world!";
    REQUIRE_EQUAL(utl::View { .offset = 0, .length = 13 }.string(string), string);
    REQUIRE_EQUAL(utl::View { .offset = 0, .length = 5 }.string(string), "Hello"sv);
    REQUIRE_EQUAL(utl::View { .offset = 7, .length = 5 }.string(string), "world"sv);
    REQUIRE_THROWS_AS(std::out_of_range, utl::View { .offset = 14, .length = 0 }.string(string));
}

UNITTEST("utl::enumerate")
{
    REQUIRE_EQUAL(
        std::vector<std::tuple<std::string_view::difference_type, char>> {
            { 0, 'h' },
            { 1, 'e' },
            { 2, 'l' },
            { 3, 'l' },
            { 4, 'o' },
        },
        std::ranges::to<std::vector>(utl::enumerate("hello"sv)));
}
