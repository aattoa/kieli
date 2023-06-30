#pragma once

#include <libutl/common/wrapper.hpp>
#include <libutl/common/pooled_string.hpp>
#include <libutl/diagnostics/diagnostics.hpp>


namespace compiler {
    struct [[nodiscard]] Shared_compilation_info {
        utl::diagnostics::Builder diagnostics;
        utl::Source::Arena        source_arena = utl::Source::Arena::with_page_size(8);
        utl::String_pool          string_literal_pool;
        utl::String_pool          operator_pool;
        utl::String_pool          identifier_pool;
    };
    using Compilation_info = utl::Explicit<std::shared_ptr<Shared_compilation_info>>;

    struct [[nodiscard]] Compile_arguments {
        std::filesystem::path source_directory_path;
        std::string           main_file_name;
    };

    auto predefinitions_source(Compilation_info&) -> utl::Wrapper<utl::Source>;
    auto mock_compilation_info(utl::diagnostics::Level = utl::diagnostics::Level::suppress) -> Compilation_info;
    auto test_info_and_source(std::string&&) -> utl::Pair<Compilation_info, utl::Source::Wrapper>;

    struct [[nodiscard]] Name_upper;
    struct [[nodiscard]] Name_lower;

    struct [[nodiscard]] Name_dynamic {
        utl::Pooled_string   identifier;
        utl::Source_view     source_view;
        utl::Explicit<bool>  is_upper;
        auto as_upper() const noexcept -> Name_upper;
        auto as_lower() const noexcept -> Name_lower;
        [[nodiscard]] auto operator==(Name_dynamic const&) const noexcept -> bool;
    };
    struct [[nodiscard]] Name_upper {
        utl::Pooled_string identifier;
        utl::Source_view   source_view;
        auto as_dynamic() const noexcept -> Name_dynamic;
        [[nodiscard]] auto operator==(Name_upper const&) const noexcept -> bool;
    };
    struct [[nodiscard]] Name_lower {
        utl::Pooled_string identifier;
        utl::Source_view   source_view;
        auto as_dynamic() const noexcept -> Name_dynamic;
        [[nodiscard]] auto operator==(Name_lower const&) const noexcept -> bool;
    };

    struct Integer   { utl::U64           value {}; };
    struct Floating  { utl::Float         value {}; };
    struct Boolean   { bool               value {}; };
    struct Character { char               value {}; };
    struct String    { utl::Pooled_string value;    };

    template <class T>
    concept literal = utl::one_of<T, Integer, Floating, Boolean, Character, String>;

    namespace built_in_type {
        enum class Integer {
            i8, i16, i32, i64,
            u8, u16, u32, u64,
            _enumerator_count,
        };
        struct Floating  {};
        struct Boolean   {};
        struct Character {};
        struct String    {};

        auto integer_string(Integer) noexcept -> std::string_view;
    }
}

template <utl::one_of<compiler::Name_dynamic, compiler::Name_lower, compiler::Name_upper> Name>
struct std::formatter<Name> : std::formatter<utl::Pooled_string> {
    auto format(Name const& name, auto& context) const {
        return std::formatter<utl::Pooled_string>::format(name.identifier, context);
    }
};

template <utl::one_of<compiler::Integer, compiler::Floating, compiler::Boolean> Literal>
struct std::formatter<Literal> : std::formatter<decltype(Literal::value)> {
    auto format(Literal const literal, auto& context) const {
        return std::formatter<decltype(Literal::value)>::format(literal.value, context);
    }
};

template <utl::one_of<compiler::Character, compiler::String> Literal>
struct std::formatter<Literal> : utl::formatting::Formatter_base {
    auto format(Literal const& literal, auto& context) const {
        if constexpr (std::is_same_v<Literal, compiler::Character>)
            return std::format_to(context.out(), "'{}'", literal.value);
        else if constexpr (std::is_same_v<Literal, compiler::String>)
            return std::format_to(context.out(), "\"{}\"", literal.value);
        else
            static_assert(utl::always_false<Literal>);
    }
};
