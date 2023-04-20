#pragma once

#include "utl/utilities.hpp"
#include "utl/source.hpp"
#include "utl/color.hpp"


namespace utl::diagnostics {

    constexpr Color line_info_color = Color::dark_cyan;
    constexpr Color note_color      = Color::cyan;
    constexpr Color warning_color   = Color::dark_yellow;
    constexpr Color error_color     = Color::red;


    enum class Level { normal, error, suppress };
    enum class Type { recoverable, irrecoverable };


    struct Text_section {
        Source_view         source_view;
        std::string_view    note = "here";
        tl::optional<Color> note_color;
    };


    class Builder {
    public:
        struct [[nodiscard]] Emit_arguments {
            std::vector<Text_section>      sections;
            std::string_view               message;
            fmt::format_args               message_arguments;
            tl::optional<std::string_view> help_note;
            fmt::format_args               help_note_arguments;
        };
        struct [[nodiscard]] Simple_emit_arguments {
            utl::Source_view               erroneous_view;
            std::string_view               message;
            fmt::format_args               message_arguments;
            tl::optional<std::string_view> help_note;
            fmt::format_args               help_note_arguments;
        };
        struct [[nodiscard]] Configuration {
            Level note_level    = Level::normal;
            Level warning_level = Level::normal;
        };
    private:
        std::string   diagnostic_string;
        Configuration configuration;
        bool          has_emitted_error;
    public:
        Builder() noexcept;
        Builder(Builder&&) noexcept;
        explicit Builder(Configuration) noexcept;

        ~Builder();

        auto emit_note       (Emit_arguments        const&) -> void;
        auto emit_simple_note(Simple_emit_arguments const&) -> void;

        auto emit_warning       (Emit_arguments        const&) -> void;
        auto emit_simple_warning(Simple_emit_arguments const&) -> void;

        [[noreturn]] auto emit_error       (Emit_arguments        const&) -> void;
        [[noreturn]] auto emit_simple_error(Simple_emit_arguments const&) -> void;

        auto emit_error       (Emit_arguments        const&, Type) -> void;
        auto emit_simple_error(Simple_emit_arguments const&, Type) -> void;

        [[nodiscard]] auto string() &&           noexcept -> std::string;
        [[nodiscard]] auto error()         const noexcept -> bool;
        [[nodiscard]] auto note_level()    const noexcept -> Level;
        [[nodiscard]] auto warning_level() const noexcept -> Level;
    };


    // Thrown when an irrecoverable diagnostic error is emitted
    struct [[nodiscard]] Error : Exception {
        using Exception::Exception;
        using Exception::operator=;
    };


    struct Message_arguments {
        std::string_view               message;
        fmt::format_args               message_arguments;
        tl::optional<std::string_view> help_note;
        fmt::format_args               help_note_arguments;

        auto add_source_view(utl::Source_view) const
            -> Builder::Simple_emit_arguments;
    };

}