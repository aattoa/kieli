#pragma once

#include <libutl/common/wrapper.hpp>
#include <libutl/common/pooled_string.hpp>
#include <libutl/source/source.hpp>
#include <cppdiag.hpp>

namespace kieli {
    // Thrown when an unrecoverable error is encountered
    struct Compilation_failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    struct Simple_text_section {
        utl::Source::Wrapper             source;
        utl::Source_range                source_range;
        std::optional<std::string_view>  note;
        std::optional<cppdiag::Severity> severity;
    };

    auto text_section(
        utl::Source::Wrapper                   section_source,
        utl::Source_range                      section_range,
        std::optional<cppdiag::Message_string> section_note = std::nullopt,
        std::optional<cppdiag::Severity>       severity = std::nullopt) -> cppdiag::Text_section;

    struct Diagnostics {
        cppdiag::Message_buffer          message_buffer;
        std::vector<cppdiag::Diagnostic> vector;
        bool                             has_emitted_error {};

        template <class... Args>
        auto emit(
            cppdiag::Severity const           severity,
            std::format_string<Args...> const fmt,
            Args&&... args) -> void
        {
            has_emitted_error = has_emitted_error || (severity == cppdiag::Severity::error);
            vector.push_back(cppdiag::Diagnostic {
                .message  = format_message(message_buffer, fmt, std::forward<Args>(args)...),
                .severity = severity,
            });
        }

        template <std::size_t n, class... Args>
        auto emit(
            cppdiag::Severity const severity,
            Simple_text_section (&&sections)[n],
            std::format_string<Args...> const fmt,
            Args&&... args) -> void
        {
            has_emitted_error = has_emitted_error || (severity == cppdiag::Severity::error);
            vector.push_back(cppdiag::Diagnostic {
                .text_sections = std::apply(
                    [&](auto&&... sections) {
                        return utl::to_vector({ text_section(
                            sections.source,
                            sections.source_range,
                            sections.note.transform([&](std::string_view const string) {
                                return cppdiag::format_message(message_buffer, "{}", string);
                            }))... });
                    },
                    std::to_array(std::move(sections))),
                .message  = format_message(message_buffer, fmt, std::forward<Args>(args)...),
                .severity = severity,
            });
        }

        template <class... Args>
        auto emit(
            cppdiag::Severity const           severity,
            utl::Source::Wrapper const        source,
            utl::Source_range const           source_range,
            std::format_string<Args...> const fmt,
            Args&&... args) -> void
        {
            emit(
                severity,
                { Simple_text_section { source, source_range } },
                fmt,
                std::forward<Args>(args)...);
        }

        template <class... Args>
        [[noreturn]] auto error(std::format_string<Args...> const fmt, Args&&... args) -> void
        {
            emit(cppdiag::Severity::error, fmt, std::forward<Args>(args)...);
            throw Compilation_failure {};
        }

        template <std::size_t n, class... Args>
        [[noreturn]] auto error(
            Simple_text_section (&&sections)[n],
            std::format_string<Args...> const fmt,
            Args&&... args) -> void
        {
            emit(cppdiag::Severity::error, std::move(sections), fmt, std::forward<Args>(args)...);
            throw Compilation_failure {};
        }

        template <class... Args>
        [[noreturn]] auto error(
            utl::Source::Wrapper const        source,
            utl::Source_range const           source_range,
            std::format_string<Args...> const fmt,
            Args&&... args) -> void
        {
            error(
                { Simple_text_section { source, source_range } }, fmt, std::forward<Args>(args)...);
        }

        [[nodiscard]] auto format_all(cppdiag::Colors) const -> std::string;
    };

    struct Compile_info {
        Diagnostics        diagnostics;
        utl::Source::Arena source_arena = utl::Source::Arena::with_page_size(8);
        utl::String_pool   string_literal_pool;
        utl::String_pool   operator_pool;
        utl::String_pool   identifier_pool;
    };

    auto predefinitions_source(Compile_info&) -> utl::Source::Wrapper;

    auto test_info_and_source(std::string&& source_string)
        -> std::pair<Compile_info, utl::Source::Wrapper>;

    struct Identifier {
        utl::Pooled_string string;
        [[nodiscard]] auto operator==(Identifier const&) const -> bool = default;
    };

    template <bool is_upper>
    struct Basic_name;
    using Name_upper = Basic_name<true>;
    using Name_lower = Basic_name<false>;

    struct Name_dynamic {
        Identifier          identifier;
        utl::Source_range   source_range;
        utl::Explicit<bool> is_upper;
        [[nodiscard]] auto  as_upper() const noexcept -> Name_upper;
        [[nodiscard]] auto  as_lower() const noexcept -> Name_lower;

        [[nodiscard]] auto operator==(Name_dynamic const& other) const noexcept -> bool
        {
            return identifier == other.identifier;
        }
    };

    template <bool is_upper>
    struct Basic_name {
        Identifier        identifier;
        utl::Source_range source_range;

        [[nodiscard]] auto as_dynamic() const -> Name_dynamic
        {
            return Name_dynamic { identifier, source_range, is_upper };
        }

        [[nodiscard]] auto operator==(Basic_name const& other) const noexcept -> bool
        {
            return identifier == other.identifier;
        }
    };

    struct Integer {
        std::uint64_t value {};
    };

    struct Floating {
        double value {};
    };

    struct Boolean {
        bool value {};
    };

    struct Character {
        char value {};
    };

    struct String {
        utl::Pooled_string value;
    };

    template <class T>
    concept literal = utl::one_of<T, Integer, Floating, Boolean, Character, String>;

    namespace built_in_type {
        enum class Integer { i8, i16, i32, i64, u8, u16, u32, u64, _enumerator_count };

        struct Floating {};

        struct Boolean {};

        struct Character {};

        struct String {};

        auto integer_name(Integer) noexcept -> std::string_view;
    } // namespace built_in_type
} // namespace kieli

template <>
struct std::formatter<kieli::Identifier> : std::formatter<utl::Pooled_string> {
    auto format(kieli::Identifier const identifier, auto& context) const
    {
        return std::formatter<utl::Pooled_string>::format(identifier.string, context);
    }
};

template <utl::one_of<kieli::Name_dynamic, kieli::Name_lower, kieli::Name_upper> Name>
struct std::formatter<Name> : std::formatter<utl::Pooled_string> {
    auto format(Name const& name, auto& context) const
    {
        return std::formatter<utl::Pooled_string>::format(name.identifier.string, context);
    }
};

template <utl::one_of<kieli::Integer, kieli::Floating, kieli::Boolean> Literal>
struct std::formatter<Literal> : std::formatter<decltype(Literal::value)> {
    auto format(Literal const literal, auto& context) const
    {
        return std::formatter<decltype(Literal::value)>::format(literal.value, context);
    }
};

template <utl::one_of<kieli::Character, kieli::String> Literal>
struct std::formatter<Literal> : utl::fmt::Formatter_base {
    auto format(Literal const& literal, auto& context) const
    {
        if constexpr (std::is_same_v<Literal, kieli::Character>) {
            return std::format_to(context.out(), "'{}'", literal.value);
        }
        else if constexpr (std::is_same_v<Literal, kieli::String>) {
            return std::format_to(context.out(), "\"{}\"", literal.value);
        }
        else {
            static_assert(false);
        }
    }
};
