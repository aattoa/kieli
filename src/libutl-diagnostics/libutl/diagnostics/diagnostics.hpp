#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <libutl/color/color.hpp>


namespace utl::diagnostics {

    constexpr Color line_info_color = Color::dark_cyan;
    constexpr Color note_color      = Color::cyan;
    constexpr Color warning_color   = Color::dark_yellow;
    constexpr Color error_color     = Color::red;

    enum class Level { normal, error, suppress };
    enum class Type { recoverable, irrecoverable };

    struct Text_section {
        Source_view         source_view;
        std::string_view    note = "Here";
        tl::optional<Color> note_color;
    };

    struct [[nodiscard]] Emit_arguments {
        std::vector<Text_section>      sections;
        std::string_view               message;
        tl::optional<std::string_view> help_note;
    };
    struct Message_arguments {
        std::string_view               message;
        tl::optional<std::string_view> help_note;

        auto add_source_view(utl::Source_view) const -> Emit_arguments;
    };

    class Builder {
    public:
        struct [[nodiscard]] Configuration {
            Level note_level    = Level::normal;
            Level warning_level = Level::normal;
        };
    private:
        std::string   m_diagnostic_string;
        utl::Usize    m_note_count {};
        utl::Usize    m_warning_count {};
        utl::Usize    m_error_count {};
        Configuration m_configuration {};
        bool          m_has_emitted_error {};
    public:
        Builder() noexcept;
        Builder(Builder&&) noexcept;
        explicit Builder(Configuration) noexcept;

        ~Builder();

        auto emit_note(Emit_arguments const&) -> void;
        auto emit_note(utl::Source_view, Message_arguments const&) -> void;

        auto emit_warning(Emit_arguments const&) -> void;
        auto emit_warning(utl::Source_view, Message_arguments const&) -> void;

        [[noreturn]] auto emit_error(Emit_arguments const&) -> void;
        [[noreturn]] auto emit_error(utl::Source_view, Message_arguments const&) -> void;

        auto emit_error(Emit_arguments const&, Type) -> void;

        [[nodiscard]] auto string() &&               noexcept -> std::string;
        [[nodiscard]] auto has_emitted_error() const noexcept -> bool;
        [[nodiscard]] auto note_level()        const noexcept -> Level;
        [[nodiscard]] auto warning_level()     const noexcept -> Level;
    };

    // Thrown when an irrecoverable diagnostic error is emitted
    struct [[nodiscard]] Error : Exception {
        using Exception::Exception;
        using Exception::operator=;
    };

}
