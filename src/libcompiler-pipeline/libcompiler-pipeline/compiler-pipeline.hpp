#pragma once

#include <libutl/common/wrapper.hpp>
#include <libutl/common/pooled_string.hpp>
#include <libutl/diagnostics/diagnostics.hpp>


namespace compiler {
    struct [[nodiscard]] Shared_compilation_info {
        utl::diagnostics::Builder       diagnostics;
        utl::Wrapper_arena<utl::Source> source_arena = utl::Source::Arena::with_page_size(8);
        utl::String_pool                string_literal_pool;
        utl::String_pool                operator_pool;
        utl::String_pool                identifier_pool;
    };
    using Compilation_info = utl::Explicit<std::shared_ptr<Shared_compilation_info>>;

    struct [[nodiscard]] Compile_arguments {
        std::filesystem::path source_directory_path;
        std::string           main_file_name;
    };

    auto predefinitions_source(Compilation_info&) -> utl::Wrapper<utl::Source>;
    auto mock_compilation_info(utl::diagnostics::Level = utl::diagnostics::Level::suppress) -> Compilation_info;

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
    struct Name_upper {
        utl::Pooled_string identifier;
        utl::Source_view   source_view;
        operator Name_dynamic() const noexcept; // NOLINT: implicit
        [[nodiscard]] auto operator==(Name_upper const&) const noexcept -> bool;
    };
    struct Name_lower {
        utl::Pooled_string identifier;
        utl::Source_view   source_view;
        operator Name_dynamic() const noexcept; // NOLINT: implicit
        [[nodiscard]] auto operator==(Name_lower const&) const noexcept -> bool;
    };

    struct Integer   { utl::U64   value {}; };
    struct Floating  { utl::Float value {}; };
    struct Boolean   { bool       value {}; };
    struct Character { char       value {}; };

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
    }
}

template <utl::one_of<compiler::Name_dynamic, compiler::Name_lower, compiler::Name_upper> Name>
struct std::formatter<Name> : std::formatter<utl::Pooled_string> {
    auto format(Name const& name, auto& context) const {
        return std::formatter<utl::Pooled_string>::format(name.identifier, context);
    }
};

template <utl::one_of<compiler::Integer, compiler::Floating, compiler::Boolean, compiler::Character> T>
struct std::formatter<T> : std::formatter<decltype(T::value)> {
    auto format(T const value_wrapper, auto& context) const {
        return std::formatter<decltype(T::value)>::format(value_wrapper.value, context);
    }
};
