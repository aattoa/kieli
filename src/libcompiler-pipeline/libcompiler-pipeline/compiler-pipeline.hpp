#pragma once

#include <libutl/common/wrapper.hpp>
#include <libutl/common/pooled_string.hpp>
#include <libutl/diagnostics/diagnostics.hpp>


namespace compiler {

    using String = utl::Pooled_string<struct _string_tag>;
    using Operator = utl::Pooled_string<struct _operator_tag>;
    using Identifier = utl::Pooled_string<struct _identifier_tag>;

    struct [[nodiscard]] Shared_compilation_info {
        utl::diagnostics::Builder       diagnostics;
        utl::Wrapper_arena<utl::Source> source_arena = utl::Source::Arena::with_page_size(8);
        String::Pool                    string_literal_pool;
        Operator::Pool                  operator_pool;
        Identifier::Pool                identifier_pool;
    };
    using Compilation_info = utl::Strong<std::shared_ptr<Shared_compilation_info>>;

    struct [[nodiscard]] Compile_arguments {
        std::filesystem::path source_directory_path;
        std::string           main_file_name;
    };

    auto predefinitions_source(Compilation_info&) -> utl::Wrapper<utl::Source>;
    auto mock_compilation_info(utl::diagnostics::Level = utl::diagnostics::Level::suppress) -> Compilation_info;


    struct [[nodiscard]] Name_upper;
    struct [[nodiscard]] Name_lower;

    struct [[nodiscard]] Name_dynamic {
        compiler::Identifier identifier;
        utl::Source_view     source_view;
        utl::Strong<bool>    is_upper;
        auto as_upper() const noexcept -> Name_upper;
        auto as_lower() const noexcept -> Name_lower;
        [[nodiscard]] auto operator==(Name_dynamic const&) const noexcept -> bool;
    };
    struct Name_upper {
        compiler::Identifier identifier;
        utl::Source_view     source_view;
        operator Name_dynamic() const noexcept; // NOLINT: implicit
        [[nodiscard]] auto operator==(Name_upper const&) const noexcept -> bool;
    };
    struct Name_lower {
        compiler::Identifier identifier;
        utl::Source_view     source_view;
        operator Name_dynamic() const noexcept; // NOLINT: implicit
        [[nodiscard]] auto operator==(Name_lower const&) const noexcept -> bool;
    };

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
struct fmt::formatter<Name> : fmt::formatter<compiler::Identifier> {
    auto format(Name const& name, auto& context) {
        return fmt::formatter<compiler::Identifier>::format(name.identifier, context);
    }
};
