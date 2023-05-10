#include <libutl/common/utilities.hpp>
#include <libutl/diagnostics/diagnostics.hpp>


namespace {

    auto remove_surrounding_whitespace(std::vector<std::string_view>& lines) -> void {
        static constexpr auto prefix_length = [](std::string_view const string) {
            return string.find_first_not_of(' ');
        };
        static_assert(prefix_length("test") == 0);
        static_assert(prefix_length("  test") == 2);

        static constexpr auto suffix_length = [](std::string_view const string) {
            return string.size() - string.find_last_not_of(' ') - 1;
        };
        static_assert(suffix_length("test") == 0);
        static_assert(suffix_length("test  ") == 2);

        utl::always_assert(!lines.empty());
        auto const shortest_prefix_length = ranges::min(lines | ranges::views::transform(prefix_length));

        for (auto& line : lines) {
            if (line.empty()) continue;
            line.remove_prefix(shortest_prefix_length);
            line.remove_suffix(suffix_length(line));
        }
    }


    auto lines_of_occurrence(std::string_view const file, std::string_view const view)
        -> std::vector<std::string_view>
    {
        utl::always_assert(!view.data() || view.size() <= std::strlen(view.data()));
        utl::always_assert(!file.data() || file.size() <= std::strlen(file.data()));

        char const* const file_start = file.data();
        char const* const file_stop  = file_start + file.size();
        char const* const view_start = view.data();
        char const* const view_stop  = view_start + view.size();

        std::vector<std::string_view> lines;

        char const* line_start = view_start;
        for (; line_start != file_start && *line_start != '\n'; --line_start);

        if (*line_start == '\n')
            ++line_start;

        for (char const* pointer = line_start; ; ++pointer) {
            if (pointer == view_stop) {
                for (; pointer != file_stop && *pointer != '\n'; ++pointer);
                lines.emplace_back(line_start, pointer);
                break;
            }
            else if (*pointer == '\n') {
                lines.emplace_back(line_start, pointer);
                line_start = pointer + 1;
            }
        }

        remove_surrounding_whitespace(lines);
        return lines;
    }


    auto format_highlighted_section(
        std::back_insert_iterator<std::string> const  out,
        utl::Color                             const  title_color,
        utl::diagnostics::Text_section         const& section) -> void
    {
        auto const lines       = lines_of_occurrence(section.source_view.source->string(), section.source_view.string);
        auto const digit_count = utl::digit_count(section.source_view.stop_position.line);
        auto       line_number = section.source_view.start_position.line;

        fmt::format_to(
            out,
            "{}{} --> {}:{}-{}{}\n",
            std::string(digit_count, ' '),
            utl::diagnostics::line_info_color,
            utl::filename_without_path(section.source_view.source->path().string()),
            section.source_view.start_position,
            section.source_view.stop_position,
            utl::Color::white);

        utl::always_assert(!lines.empty());
        utl::Usize const longest_line_length =
            ranges::max(lines | ranges::views::transform(utl::size));

        for (auto const& line : lines) {
            fmt::format_to(
                out,
                "\n {}{:<{}} |{} ",
                utl::diagnostics::line_info_color,
                line_number++,
                digit_count,
                utl::Color::white,
                line);

            if (lines.size() > 1) {
                if (&line == &lines.front()) {
                    utl::Usize const source_view_offset =
                        utl::unsigned_distance(line.data(), section.source_view.string.data());

                    fmt::format_to(
                        out,
                        "{}{}{}{}",
                        utl::Color::dark_grey,
                        line.substr(0, source_view_offset),
                        utl::Color::white,
                        line.substr(source_view_offset));
                }
                else if (&line == &lines.back()) {
                    utl::Usize const source_view_offset =
                        utl::unsigned_distance(
                            line.data(),
                            section.source_view.string.data() + section.source_view.string.size());

                    fmt::format_to(
                        out,
                        "{}{}{}{}",
                        line.substr(0, source_view_offset),
                        utl::Color::dark_grey,
                        line.substr(source_view_offset),
                        utl::Color::white);
                }
                else {
                    fmt::format_to(out, "{}", line);
                }
            }
            else {
                fmt::format_to(out, "{}", line);
            }

            if (lines.size() > 1) {
                fmt::format_to(
                    out,
                    "{} {}<",
                    std::string(longest_line_length - line.size(), ' '),
                    section.note_color.value_or(title_color));

                if (&line == &lines.back())
                    fmt::format_to(out, " {}", section.note);

                fmt::format_to(out, "{}", utl::Color::white);
            }
        }

        if (lines.size() == 1) {
            auto whitespace_length
                = section.source_view.string.size()
                + digit_count
                + utl::unsigned_distance(
                    lines.front().data(),
                    section.source_view.string.data());

            if (section.source_view.string.empty())
                // Only reached if the error occurred at the end of input
                ++whitespace_length;

            fmt::format_to(
                out,
                "\n    {}{:>{}} {}{}",
                section.note_color.value_or(title_color),
                std::string(std::max(section.source_view.string.size(), 1_uz), '^'),
                whitespace_length,
                section.note,
                utl::Color::white);
        }
    }


    auto do_emit(
        std::string                                    & diagnostic_string,
        utl::diagnostics::Builder::Emit_arguments const& arguments,
        std::string_view                          const  title,
        utl::Color                                const  title_color,
        utl::diagnostics::Type                    const  diagnostic_type = utl::diagnostics::Type::recoverable) -> void
    {
        auto const& [sections, message, message_format_arguments, help_note, help_note_arguments] = arguments;
        auto out = std::back_inserter(diagnostic_string);

        if (!diagnostic_string.empty())
            // There are previous diagnostic messages, insert newlines to separate them
            fmt::format_to(out, "\n\n\n");

        fmt::format_to(out, "{}{}: {}", title_color, title, utl::Color::white);
        fmt::vformat_to(out, message, message_format_arguments);

        if (!sections.empty())
            fmt::format_to(out, "\n\n");

        for (auto const& section : sections) {
            format_highlighted_section(out, title_color, section);
            if (&section != &sections.back())
                fmt::format_to(out, "\n\n");
        }

        if (help_note) {
            fmt::format_to(out, "\n\nHelpful note: ");
            fmt::vformat_to(out, *help_note, help_note_arguments);
        }

        if (diagnostic_type == utl::diagnostics::Type::irrecoverable)
            throw utl::diagnostics::Error { std::move(diagnostic_string) };
    }


    auto to_regular_args(
        utl::diagnostics::Builder::Simple_emit_arguments const& arguments,
        utl::Color                                       const  note_color) -> utl::diagnostics::Builder::Emit_arguments
    {
        return {
            .sections {
                utl::diagnostics::Text_section {
                    .source_view = arguments.erroneous_view,
                    .note        = "here",
                    .note_color  = note_color,
                }
            },
            .message             = arguments.message,
            .message_arguments   = arguments.message_arguments,
            .help_note           = arguments.help_note,
            .help_note_arguments = arguments.help_note_arguments,
        };
    }

}


utl::diagnostics::Builder::Builder() noexcept
    : Builder { Configuration {} } {}

utl::diagnostics::Builder::Builder(Configuration const configuration) noexcept
    : m_configuration { configuration }
    , m_has_emitted_error { false } {}

utl::diagnostics::Builder::Builder(Builder&& other) noexcept
    : m_diagnostic_string { std::move(other.m_diagnostic_string) }
    , m_configuration     { other.m_configuration }
    , m_has_emitted_error { other.m_has_emitted_error }
{
    other.m_diagnostic_string.clear();
    other.m_has_emitted_error = false;
    // A moved-from std::string is not guaranteed to be empty, so
    // this move constructor has to be written by hand, because the
    // destructor relies on moved-from diagnostic strings being empty.
}

utl::diagnostics::Builder::~Builder() {
    if (!m_diagnostic_string.empty())
        std::cout << m_diagnostic_string << "\n\n"; // TODO: improve
}

auto utl::diagnostics::Builder::string() && noexcept -> std::string {
    return std::move(m_diagnostic_string);
}
auto utl::diagnostics::Builder::has_emitted_error() const noexcept -> bool {
    return m_has_emitted_error;
}
auto utl::diagnostics::Builder::note_level() const noexcept -> Level {
    return m_configuration.note_level;
}
auto utl::diagnostics::Builder::warning_level() const noexcept -> Level {
    return m_configuration.warning_level;
}


auto utl::diagnostics::Builder::emit_note(Emit_arguments const& arguments) -> void {
    switch (m_configuration.note_level) {
    case Level::normal:
        return do_emit(m_diagnostic_string, arguments, "Note", note_color);
    case Level::error:
        return do_emit(m_diagnostic_string, arguments, "The following note is treated as an error", error_color);
    case Level::suppress:
        return;
    default:
        utl::unreachable();
    }
}

auto utl::diagnostics::Builder::emit_simple_note(Simple_emit_arguments const& arguments) -> void {
    return emit_note(to_regular_args(arguments, note_color));
}


auto utl::diagnostics::Builder::emit_warning(Emit_arguments const& arguments) -> void {
    switch (m_configuration.warning_level) {
    case Level::normal:
        return do_emit(m_diagnostic_string, arguments, "Warning", warning_color);
    case Level::error:
        m_has_emitted_error = true;
        return do_emit(m_diagnostic_string, arguments, "The following warning is treated as an error", error_color);
    case Level::suppress:
        return;
    default:
        utl::unreachable();
    }
}

auto utl::diagnostics::Builder::emit_simple_warning(Simple_emit_arguments const& arguments) -> void {
    return emit_warning(to_regular_args(arguments, warning_color));
}


auto utl::diagnostics::Builder::emit_error(Emit_arguments const& arguments, Type const  error_type) -> void {
    m_has_emitted_error = true;
    do_emit(m_diagnostic_string, arguments, "Error", error_color, error_type);
}
auto utl::diagnostics::Builder::emit_error(Emit_arguments const& arguments) -> void {
    emit_error(arguments, Type::irrecoverable);
    utl::unreachable();
}

auto utl::diagnostics::Builder::emit_simple_error(Simple_emit_arguments const& arguments, Type const error_type) -> void {
    emit_error(to_regular_args(arguments, error_color), error_type);
}
auto utl::diagnostics::Builder::emit_simple_error(Simple_emit_arguments const& arguments) -> void {
    emit_simple_error(arguments, Type::irrecoverable);
    utl::unreachable();
}


auto utl::diagnostics::Message_arguments::add_source_view(utl::Source_view const erroneous_view) const -> Builder::Simple_emit_arguments {
    return {
        .erroneous_view      = erroneous_view,
        .message             = message,
        .message_arguments   = message_arguments,
        .help_note           = help_note,
        .help_note_arguments = help_note_arguments,
    };
}
