#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libdesugar/desugar.hpp>

namespace libdesugar {
    namespace cst = kieli::cst;
    namespace ast = kieli::ast;
    using Context = kieli::Desugar_context;

    auto desugar(Context, cst::Definition const&) -> ast::Definition;
    auto desugar(Context, cst::Expression const&) -> ast::Expression;
    auto desugar(Context, cst::Pattern const&) -> ast::Pattern;
    auto desugar(Context, cst::Type const&) -> ast::Type;
    auto desugar(Context, cst::Function_signature const&) -> ast::Function_signature;
    auto desugar(Context, cst::definition::Function const&) -> ast::definition::Function;
    auto desugar(Context, cst::definition::Struct const&) -> ast::definition::Enumeration;
    auto desugar(Context, cst::definition::Enum const&) -> ast::definition::Enumeration;
    auto desugar(Context, cst::definition::Alias const&) -> ast::definition::Alias;
    auto desugar(Context, cst::definition::Concept const&) -> ast::definition::Concept;
    auto desugar(Context, cst::definition::Impl const&) -> ast::definition::Impl;
    auto desugar(Context, cst::definition::Submodule const&) -> ast::definition::Submodule;

    auto desugar(Context, cst::Function_parameters const&) -> std::vector<ast::Function_parameter>;
    auto desugar(Context, cst::Template_argument const&) -> ast::Template_argument;
    auto desugar(Context, cst::Template_parameter const&) -> ast::Template_parameter;
    auto desugar(Context, cst::Mutability const&) -> ast::Mutability;
    auto desugar(Context, cst::Path_segment const&) -> ast::Path_segment;
    auto desugar(Context, cst::Path const&) -> ast::Path;
    auto desugar(Context, cst::Type_annotation const&) -> ast::Type_id;
    auto desugar(Context, cst::Type_signature const&) -> ast::Type_signature;
    auto desugar(Context, cst::Struct_field_initializer const&) -> ast::Struct_field_initializer;
    auto desugar(Context, cst::pattern::Field const&) -> ast::pattern::Field;
    auto desugar(Context, cst::pattern::Constructor_body const&) -> ast::pattern::Constructor_body;
    auto desugar(Context, cst::definition::Field const&) -> ast::definition::Field;
    auto desugar(Context, cst::definition::Constructor_body const&)
        -> ast::definition::Constructor_body;
    auto desugar(Context, cst::definition::Constructor const&) -> ast::definition::Constructor;
    auto desugar(Context, cst::Wildcard const&) -> ast::Wildcard;
    auto desugar(Context, cst::Type_parameter_default_argument const&)
        -> ast::Template_type_parameter::Default;
    auto desugar(Context, cst::Value_parameter_default_argument const&)
        -> ast::Template_value_parameter::Default;
    auto desugar(Context, cst::Mutability_parameter_default_argument const&)
        -> ast::Template_mutability_parameter::Default;
    auto desugar(Context, cst::Expression_id) -> ast::Expression_id;
    auto desugar(Context, cst::Pattern_id) -> ast::Pattern_id;
    auto desugar(Context, cst::Type_id) -> ast::Type_id;

    template <class T>
    auto desugar(Context, std::vector<T> const&);
    template <class T>
    auto desugar(Context, cst::Separated_sequence<T> const&);
    template <class T>
    auto desugar(Context, cst::Surrounded<T> const&);

    inline auto desugar(Context const context) noexcept
    {
        return [=](auto const& sugared) { return desugar(context, sugared); };
    }

    template <class T>
    auto desugar(Context const context, std::vector<T> const& vector)
    {
        return std::ranges::to<std::vector>(std::views::transform(vector, desugar(context)));
    }

    template <class T>
    auto desugar(Context const context, cst::Separated_sequence<T> const& sequence)
    {
        return desugar(context, sequence.elements);
    }

    template <class T>
    auto desugar(Context const context, cst::Surrounded<T> const& surrounded)
    {
        return desugar(context, surrounded.value);
    }

    auto wrap_desugar(Context, cst::Expression const&) -> ast::Expression_id;
    auto wrap_desugar(Context, cst::Pattern const&) -> ast::Pattern_id;
    auto wrap_desugar(Context, cst::Type const&) -> ast::Type_id;

    auto deref_desugar(Context, cst::Expression_id) -> ast::Expression;
    auto deref_desugar(Context, cst::Pattern_id) -> ast::Pattern;
    auto deref_desugar(Context, cst::Type_id) -> ast::Type;

    inline auto wrap_desugar(Context const context) noexcept
    {
        return [=](auto const& sugared) { return wrap_desugar(context, sugared); };
    }

    inline auto deref_desugar(Context const context) noexcept
    {
        return [=](auto const& sugared) { return deref_desugar(context, sugared); };
    }

    auto desugar_mutability(std::optional<cst::Mutability> const&, kieli::Range) -> ast::Mutability;
    auto desugar_mutability(cst::Mutability const&) -> ast::Mutability;

    auto unit_type(kieli::Range) -> ast::Type;
    auto wildcard_type(kieli::Range) -> ast::Type;
    auto unit_value(kieli::Range) -> ast::Expression;
    auto wildcard_pattern(kieli::Range) -> ast::Pattern;
} // namespace libdesugar
