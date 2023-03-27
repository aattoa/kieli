#include "utl/utilities.hpp"
#include "color.hpp"


namespace {

    constinit bool color_formatting_state = true;

    auto color_string(utl::Color const color) noexcept -> std::string_view {
        static constexpr auto color_map = std::to_array<std::string_view>({
            "\033[31m",       // dark red
            "\033[32m",       // dark green
            "\033[33m",       // dark yellow
            "\033[34m",       // dark blue
            "\033[35m",       // dark purple
            "\033[36m",       // dark cyan
            "\033[38;5;238m", // dark grey

            "\033[91m", // red
            "\033[92m", // green
            "\033[93m", // yellow
            "\033[94m", // blue
            "\033[95m", // purple
            "\033[96m", // cyan
            "\033[90m", // grey

            "\033[30m", // black
            "\033[0m" , // white
        });
        static_assert(color_map.size() == utl::enumerator_count<utl::Color>);
        return color_map[utl::as_index(color)];
    }

}


#ifdef _WIN32

// Copied the necessary declarations from Windows.h, this allows not #including it,
// which would fail to compile because it requires some MS-specific compiler extensions.

extern "C" {
    typedef unsigned long DWORD;
    typedef int           BOOL;
    typedef void*         HANDLE;
    typedef DWORD*        LPDWORD;
    typedef long long     LONG_PTR;

    HANDLE __declspec(dllimport) GetStdHandle(DWORD);
    BOOL   __declspec(dllimport) SetConsoleMode(HANDLE, DWORD);
    BOOL   __declspec(dllimport) GetConsoleMode(HANDLE, LPDWORD);
}

#define STD_OUTPUT_HANDLE                  ((DWORD)-11)
#define INVALID_HANDLE_VALUE               ((HANDLE)(LONG_PTR)-1)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004


namespace {

    auto enable_virtual_terminal_processing() -> void {
        static bool has_been_enabled = false;

        if (!has_been_enabled) {
            has_been_enabled = true;

            // If the win32api calls fail, silently fall back and disable color formatting

            HANDLE const console = GetStdHandle(STD_OUTPUT_HANDLE);
            if (console == INVALID_HANDLE_VALUE) {
                color_formatting_state = false;
                return;
            }

            DWORD mode;
            if (!GetConsoleMode(console, &mode)) {
                color_formatting_state = false;
                return;
            }

            if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                if (!SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                    color_formatting_state = false;
                    return;
                }
            }
        }
    }

}

#else
#define enable_virtual_terminal_processing() ((void)0)
#endif


auto utl::enable_color_formatting() noexcept -> void {
    color_formatting_state = true;
}
auto utl::disable_color_formatting() noexcept -> void {
    color_formatting_state = false;
}


auto operator<<(std::ostream& os, utl::Color const color) -> std::ostream& {
    if (color_formatting_state) {
        enable_virtual_terminal_processing();
        return os << color_string(color);
    }
    return os;
}

DEFINE_FORMATTER_FOR(utl::Color) {
    if (color_formatting_state) {
        enable_virtual_terminal_processing();
        return fmt::format_to(context.out(), "{}", color_string(value));
    }
    return context.out();
}