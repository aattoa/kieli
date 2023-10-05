#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/color/color.hpp>

namespace utl::diag {

    struct Position {
        utl::Usize line   = 1;
        utl::Usize column = 1;

        auto operator<=>(Position const&) const = default;
    };

    struct Text_section {
        std::string_view               source_string;
        Position                       start_position;
        Position                       stop_position;
        std::optional<Relative_string> note;
        std::optional<Color>           note_color;
    };

    enum class Level { error, warning, note };

    struct Diagnostic_arguments {
        std::vector<Text_section>      text_sections;
        std::optional<Relative_string> message;
        std::optional<Relative_string> help_note;
        utl::Explicit<Level>           level;
    };

    struct Diagnostic {
        // TODO
        Level level;
    };

    struct Context {
        struct Colors {
            Color level_error   = Color::red;
            Color level_warning = Color::dark_yellow;
            Color level_note    = Color::cyan;
            Color position_info = Color::dark_cyan;
        };

        std::string diagnostics_buffer;
        Colors      colors;

        utl::Usize error_count {};
        utl::Usize warning_count {};
        utl::Usize note_count {};

        template <class... Args>
        auto format(std::format_string<Args...> const fmt, Args&&... args) -> Relative_string
        {
            return Relative_string::format_to(diagnostics_buffer, fmt, std::forward<Args>(args)...);
        }

        auto make_diagnostic(Diagnostic_arguments const&) -> Diagnostic;
    };

} // namespace utl::diag
