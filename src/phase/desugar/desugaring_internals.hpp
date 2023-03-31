#pragma once

#include "utl/utilities.hpp"
#include "utl/safe_integer.hpp"
#include "utl/diagnostics.hpp"
#include "representation/ast/ast.hpp"
#include "representation/hir/hir.hpp"
#include "phase/lex/lex.hpp"


class Desugaring_context {
    utl::Safe_usize current_name_tag;
    utl::Usize      current_definition_kind = std::variant_size_v<ast::Definition::Variant>;
public:
    hir::Node_context            & node_context;
    utl::diagnostics::Builder    & diagnostics;
    utl::Source             const& source;
    compiler::Program_string_pool& string_pool;

    std::vector<hir::Implicit_template_parameter>* current_function_implicit_template_parameters = nullptr;
    compiler::Identifier                           self_variable_identifier = string_pool.identifiers.make("self");

    Desugaring_context(
        hir::Node_context            &,
        utl::diagnostics::Builder    &,
        utl::Source             const&,
        compiler::Program_string_pool&) noexcept;

    [[nodiscard]]
    auto is_within_function() const noexcept -> bool;

    [[nodiscard]]
    auto fresh_name_tag() -> utl::Usize;

    auto desugar(ast::Expression const&) -> hir::Expression;
    auto desugar(ast::Type       const&) -> hir::Type;
    auto desugar(ast::Pattern    const&) -> hir::Pattern;
    auto desugar(ast::Definition const&) -> hir::Definition;

    auto desugar(ast::Function_argument           const&) -> hir::Function_argument;
    auto desugar(ast::Function_parameter          const&) -> hir::Function_parameter;
    auto desugar(ast::Template_argument           const&) -> hir::Template_argument;
    auto desugar(ast::Template_parameter          const&) -> hir::Template_parameter;
    auto desugar(ast::Qualifier                   const&) -> hir::Qualifier;
    auto desugar(ast::Qualified_name              const&) -> hir::Qualified_name;
    auto desugar(ast::Class_reference             const&) -> hir::Class_reference;
    auto desugar(ast::Function_signature          const&) -> hir::Function_signature;
    auto desugar(ast::Function_template_signature const&) -> hir::Function_template_signature;
    auto desugar(ast::Type_signature              const&) -> hir::Type_signature;
    auto desugar(ast::Type_template_signature     const&) -> hir::Type_template_signature;

    auto desugar(utl::wrapper auto const node) {
        return utl::wrap(desugar(*node));
    }

    auto desugar() noexcept {
        return [this](auto const& node) {
            return desugar(node);
        };
    }

    auto unit_value      (utl::Source_view) -> utl::Wrapper<hir::Expression>;
    auto wildcard_pattern(utl::Source_view) -> utl::Wrapper<hir::Pattern>;
    auto true_pattern    (utl::Source_view) -> utl::Wrapper<hir::Pattern>;
    auto false_pattern   (utl::Source_view) -> utl::Wrapper<hir::Pattern>;

    [[noreturn]]
    auto error(utl::Source_view, utl::diagnostics::Message_arguments) const -> void;
};