#pragma once

#include <libutl/common/wrapper.hpp>
#include <libutl/common/pooled_string.hpp>
#include <libutl/source/source.hpp>
#include <cppdiag.hpp>

namespace kieli {
    auto text_section(
        utl::Source_view                       section_view,
        std::optional<cppdiag::Message_string> section_note = std::nullopt,
        std::optional<cppdiag::Severity>       severity = std::nullopt) -> cppdiag::Text_section;

    // Thrown when an unrecoverable error is encountered
    struct Compilation_failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    struct Diagnostics {
        cppdiag::Context                 context;
        std::vector<cppdiag::Diagnostic> vector;

        template <class... Args>
        [[noreturn]] auto error(
            utl::Source_view const view, std::format_string<Args...> const fmt, Args&&... args)
            -> void
        {
            vector.push_back(cppdiag::Diagnostic {
                .text_sections = utl::to_vector({ text_section(view) }),
                .message       = context.format_message(fmt, std::forward<Args>(args)...),
                .severity      = cppdiag::Severity::error,
            });
            throw Compilation_failure {};
        }

        [[nodiscard]] auto format_all(cppdiag::Colors) const -> std::string;
    };

    struct Shared_compilation_info {
        Diagnostics        diagnostics;
        utl::Source::Arena source_arena = utl::Source::Arena::with_page_size(8);
        utl::String_pool   string_literal_pool;
        utl::String_pool   operator_pool;
        utl::String_pool   identifier_pool;
    };

    using Compilation_info = utl::Explicit<std::shared_ptr<Shared_compilation_info>>;

    auto predefinitions_source(Compilation_info&) -> utl::Source::Wrapper;

    auto test_info_and_source(std::string&& source_string)
        -> utl::Pair<Compilation_info, utl::Source::Wrapper>;

    struct [[nodiscard]] Name_upper;
    struct [[nodiscard]] Name_lower;

    struct [[nodiscard]] Name_dynamic {
        utl::Pooled_string  identifier;
        utl::Source_view    source_view;
        utl::Explicit<bool> is_upper;
        auto                as_upper() const noexcept -> Name_upper;
        auto                as_lower() const noexcept -> Name_lower;
        [[nodiscard]] auto  operator==(Name_dynamic const&) const noexcept -> bool;
    };

    struct [[nodiscard]] Name_upper {
        utl::Pooled_string identifier;
        utl::Source_view   source_view;
        auto               as_dynamic() const noexcept -> Name_dynamic;
        [[nodiscard]] auto operator==(Name_upper const&) const noexcept -> bool;
    };

    struct [[nodiscard]] Name_lower {
        utl::Pooled_string identifier;
        utl::Source_view   source_view;
        auto               as_dynamic() const noexcept -> Name_dynamic;
        [[nodiscard]] auto operator==(Name_lower const&) const noexcept -> bool;
    };

    struct Integer {
        utl::U64 value {};
    };

    struct Floating {
        utl::Float value {};
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

        auto integer_string(Integer) noexcept -> std::string_view;
    } // namespace built_in_type
} // namespace kieli

template <utl::one_of<kieli::Name_dynamic, kieli::Name_lower, kieli::Name_upper> Name>
struct std::formatter<Name> : std::formatter<utl::Pooled_string> {
    auto format(Name const& name, auto& context) const
    {
        return std::formatter<utl::Pooled_string>::format(name.identifier, context);
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
struct std::formatter<Literal> : utl::formatting::Formatter_base {
    auto format(Literal const& literal, auto& context) const
    {
        if constexpr (std::is_same_v<Literal, kieli::Character>) {
            return std::format_to(context.out(), "'{}'", literal.value);
        }
        else if constexpr (std::is_same_v<Literal, kieli::String>) {
            return std::format_to(context.out(), "\"{}\"", literal.value);
        }
        else {
            static_assert(utl::always_false<Literal>);
        }
    }
};
