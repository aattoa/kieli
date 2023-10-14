#include <libutl/common/utilities.hpp>
#include <libutl/diag/internal/diag.hpp>

namespace {
    // A position is valid if both its line and column components are nonzero.
    constexpr auto is_valid_position(utl::diag::Position const position) noexcept -> bool
    {
        return position.line != 0 && position.column != 0;
    }

    auto level_color(utl::diag::Level const level, utl::diag::Colors const colors) noexcept
        -> utl::Color
    {
        switch (level) {
        case utl::diag::Level::error:
            return colors.error;
        case utl::diag::Level::warning:
            return colors.warning;
        case utl::diag::Level::note:
            return colors.note;
        default:
            utl::unreachable();
        }
    }

    auto level_string(utl::diag::Level const level) noexcept -> std::string_view
    {
        switch (level) {
        case utl::diag::Level::error:
            return "Error";
        case utl::diag::Level::warning:
            return "Warning";
        case utl::diag::Level::note:
            return "Note";
        default:
            utl::unreachable();
        }
    }

    auto format_section(utl::diag::Text_section const& section, std::string& output) -> void
    {
        (void)section;
        (void)output;
        utl::todo();
    }
} // namespace

auto utl::diag::Colors::defaults() noexcept -> Colors
{
    return Colors {
        .normal        = Color::white,
        .error         = Color::red,
        .warning       = Color::dark_yellow,
        .note          = Color::cyan,
        .position_info = Color::dark_cyan,
    };
}

auto utl::diag::internal::get_relevant_lines(
    std::string_view const source_string, Position const section_start, Position const section_stop)
    -> std::vector<std::string_view>
{
    utl::always_assert(!source_string.empty());
    utl::always_assert(is_valid_position(section_start));
    utl::always_assert(is_valid_position(section_stop));
    utl::always_assert(section_start <= section_stop);

    auto       source_it  = source_string.begin();
    auto const source_end = source_string.end();

    if (section_start.line != 1) {
        source_it = utl::find_nth(source_it, source_end, section_start.line - 2, '\n');
        // If a newline isn't found, the position was set incorrectly.
        utl::always_assert(source_it != source_end);
        // Set the source iterator to the first character of the first relevant line.
        ++source_it;
    }

    std::vector<std::string_view> lines;
    lines.reserve(1 + section_stop.line - section_start.line);

    for (utl::Usize line = section_start.line; line != section_stop.line; ++line) {
        auto const newline_it = std::find(source_it, source_end, '\n');
        // As the last relevant line is yet to be reached, a newline must be found.
        utl::always_assert(newline_it != source_end);
        lines.emplace_back(source_it, newline_it);
        // Set the source iterator to the first character of the next relevant line.
        source_it = newline_it + 1;
    }

    // The final line can be terminated by either a newline or the end of input.
    lines.emplace_back(source_it, std::find(source_it, source_end, '\n'));

    return lines;
}

auto utl::diag::Context::format_diagnostic(
    Diagnostic const& diagnostic, std::string& output, Colors const colors) -> void
{
    auto const original_output_size = output.size();
    try {
        std::format_to(
            std::back_inserter(output),
            "{}{}:{} {}",
            level_color(diagnostic.level, colors),
            level_string(diagnostic.level),
            colors.normal,
            diagnostic.message.view_in(m_diagnostics_buffer));

        for (Text_section const& section : diagnostic.text_sections) {
            format_section(section, output);
        }

        if (diagnostic.help_note.has_value()) {
            std::format_to(
                std::back_inserter(output),
                "\n\n{}",
                diagnostic.help_note->view_in(m_diagnostics_buffer));
        }
    }
    catch (...) {
        output.resize(original_output_size);
        throw;
    }
}

auto utl::diag::Context::format_diagnostic(Diagnostic const& diagnostic, Colors const colors)
    -> std::string
{
    auto output = utl::string_with_capacity(64);
    format_diagnostic(diagnostic, output, colors);
    return output;
}
