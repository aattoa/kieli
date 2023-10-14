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

    struct Diagnostic {
        std::vector<Text_section>      text_sections;
        Relative_string                message;
        std::optional<Relative_string> help_note;
        utl::Explicit<Level>           level;
    };

    struct Colors {
        Color normal {};
        Color error {};
        Color warning {};
        Color note {};
        Color position_info {};

        static auto defaults() noexcept -> Colors;
    };

    class Context {
        std::string m_diagnostics_buffer;
    public:
        // Format `diagnostic` to `output` according to `colors`.
        auto format_diagnostic(
            Diagnostic const& diagnostic, std::string& output, Colors colors = Colors::defaults())
            -> void;

        // Format `diagnostic` to a new string according to `colors`.
        auto format_diagnostic(Diagnostic const& diagnostic, Colors colors = Colors::defaults())
            -> std::string;

        template <class... Args>
        auto format_relative(std::format_string<Args...> const fmt, Args&&... args)
            -> Relative_string
        {
            return Relative_string::format_to(
                m_diagnostics_buffer, fmt, std::forward<Args>(args)...);
        }
    };

} // namespace utl::diag
