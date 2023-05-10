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

    auto  enable_color_formatting() noexcept -> void;
    auto disable_color_formatting() noexcept -> void;

}


auto operator<<(std::ostream&, utl::Color) -> std::ostream&;

DECLARE_FORMATTER_FOR(utl::Color);

