#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/filesystem.hpp>
#include <cpputil/mem/stable_string_pool.hpp>
#include <cppdiag/cppdiag.hpp>

namespace kieli {
    // Thrown when an unrecoverable error is encountered
    // TODO: Remove this!
    struct Compilation_failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    struct Revision : utl::Explicit<std::size_t> {
        using Explicit::Explicit, Explicit::operator=;
    };

    struct Database {
        std::vector<cppdiag::Diagnostic> diagnostics;
        Source_vector                    sources;
        cpputil::mem::Stable_string_pool string_pool;
        Revision                         current_revision;
    };

    auto emit_diagnostic(
        cppdiag::Severity          severity,
        Database&                  db,
        Source_id                  source,
        Range                      range,
        std::string                message,
        std::optional<std::string> help_note = std::nullopt) -> void;

    [[noreturn]] auto fatal_error(
        Database&                  db,
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

    auto predefinitions_source(Database&) -> Source_id;

    using Identifier = cpputil::mem::Stable_pool_string;

    struct Name {
        Identifier identifier;
        Range      range;

        // Check whether this name starts with an upper-case letter,
        // possibly preceded by underscores.
        [[nodiscard]] auto is_upper() const -> bool;

        auto operator==(Name const&) const -> bool = default;
    };

    struct Upper : Name {};

    struct Lower : Name {};

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
        cpputil::mem::Stable_pool_string value;
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
struct std::formatter<kieli::Name> : std::formatter<std::string_view> {
    auto format(kieli::Name const& name, auto& context) const
    {
        return std::formatter<std::string_view>::format(name.identifier.view(), context);
    }
};

template <utl::one_of<kieli::Upper, kieli::Lower> X>
struct std::formatter<X> : std::formatter<std::string_view> {
    auto format(X const& x, auto& context) const
    {
        return std::formatter<std::string_view>::format(x.identifier.view(), context);
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
