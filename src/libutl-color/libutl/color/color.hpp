#pragma once

#include <libutl/common/utilities.hpp>

namespace utl {

    enum class Color {
        dark_red,
        dark_green,
        dark_yellow,
        dark_blue,
        dark_purple,
        dark_cyan,
        dark_grey,

        red,
        green,
        yellow,
        blue,
        purple,
        cyan,
        grey,

        black,
        white,

        _enumerator_count
    };

    auto color_string(utl::Color) noexcept -> std::string_view;

    auto set_color_formatting_state(bool) noexcept -> void;

    auto operator<<(std::ostream&, Color) -> std::ostream&;

} // namespace utl

template <>
struct std::formatter<utl::Color> : std::formatter<std::string_view> {
    auto format(utl::Color const& color, auto& context) const
    {
        return std::formatter<std::string_view>::format(utl::color_string(color), context);
    }
};
