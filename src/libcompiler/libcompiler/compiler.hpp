#pragma once

#include <libutl/wrapper.hpp>
#include <libutl/pooled_string.hpp>
#include <libcompiler/filesystem.hpp>
#include <cppdiag/cppdiag.hpp>

namespace kieli {
    // Thrown when an unrecoverable error is encountered
    // TODO: Remove this!
    struct Compilation_failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    struct Compile_info {
        std::vector<cppdiag::Diagnostic> diagnostics;
        Source_vector                    source_vector;
        utl::String_pool                 string_literal_pool;
        utl::String_pool                 operator_pool;
        utl::String_pool                 identifier_pool;
    };

    auto emit_diagnostic(
        cppdiag::Severity          severity,
        Compile_info&              info,
        Source_id                  source,
        Range                      error_range,
        std::string                message,
        std::optional<std::string> help_note = std::nullopt) -> void;

    [[noreturn]] auto fatal_error(
        Compile_info&              info,
        Source_id                  source,
        Range                      error_range,
        std::string                message,
        std::optional<std::string> help_note = std::nullopt) -> void;

    auto text_section(
        Source const&                    source,
        Range                            range,
        std::optional<std::string>       note          = std::nullopt,
        std::optional<cppdiag::Severity> note_severity = std::nullopt) -> cppdiag::Text_section;

    auto format_diagnostics(
        std::span<cppdiag::Diagnostic const> diagnostics,
        cppdiag::Colors                      colors = cppdiag::Colors::defaults()) -> std::string;

    auto test_info_and_source(std::string&& source_string) -> std::pair<Compile_info, Source_id>;

    auto predefinitions_source(Compile_info&) -> Source_id;

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
        Range               range;
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
        Identifier identifier;
        Range      range;

        [[nodiscard]] auto as_dynamic() const -> Name_dynamic
        {
            return Name_dynamic { identifier, range, is_upper };
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

    enum class Mutability { mut, immut };

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
struct std::formatter<Literal> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(Literal const& literal, auto& context)
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

template <>
struct std::formatter<kieli::Mutability> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(kieli::Mutability const mut, auto& context)
    {
        return std::format_to(context.out(), "{}", mut == kieli::Mutability::mut ? "mut" : "immut");
    }
};
