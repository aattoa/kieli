#pragma once

#include "utl/utilities.hpp"
#include "utl/safe_integer.hpp"
#include "utl/diagnostics.hpp"
#include "ast/ast.hpp"
#include "hir/hir.hpp"
#include "lexer/lexer.hpp"


class Lowering_context {
    utl::Safe_usize current_name_tag;
    utl::Usize      current_definition_kind = std::variant_size_v<ast::Definition::Variant>;
public:
    hir::Node_context            & node_context;
    utl::diagnostics::Builder     & diagnostics;
    utl::Source              const& source;
    compiler::Program_string_pool& string_pool;

    std::vector<hir::Implicit_template_parameter>* current_function_implicit_template_parameters = nullptr;
    compiler::Identifier                           self_variable_identifier = string_pool.identifiers.make("self");

    Lowering_context(
        hir::Node_context            &,
        utl::diagnostics::Builder     &,
        utl::Source              const&,
        compiler::Program_string_pool&
    ) noexcept;

    auto is_within_function() const noexcept -> bool;

    auto fresh_name_tag() -> utl::Usize;

    auto lower(ast::Expression const&) -> hir::Expression;
    auto lower(ast::Type       const&) -> hir::Type;
    auto lower(ast::Pattern    const&) -> hir::Pattern;
    auto lower(ast::Definition const&) -> hir::Definition;

    auto lower(ast::Function_argument           const&) -> hir::Function_argument;
    auto lower(ast::Function_parameter          const&) -> hir::Function_parameter;
    auto lower(ast::Template_argument           const&) -> hir::Template_argument;
    auto lower(ast::Template_parameter          const&) -> hir::Template_parameter;
    auto lower(ast::Qualifier                   const&) -> hir::Qualifier;
    auto lower(ast::Qualified_name              const&) -> hir::Qualified_name;
    auto lower(ast::Class_reference             const&) -> hir::Class_reference;
    auto lower(ast::Function_signature          const&) -> hir::Function_signature;
    auto lower(ast::Function_template_signature const&) -> hir::Function_template_signature;
    auto lower(ast::Type_signature              const&) -> hir::Type_signature;
    auto lower(ast::Type_template_signature     const&) -> hir::Type_template_signature;

    auto lower(utl::wrapper auto const node) {
        return utl::wrap(lower(*node));
    }

    auto unit_value      (utl::Source_view) -> utl::Wrapper<hir::Expression>;
    auto wildcard_pattern(utl::Source_view) -> utl::Wrapper<hir::Pattern>;
    auto true_pattern    (utl::Source_view) -> utl::Wrapper<hir::Pattern>;
    auto false_pattern   (utl::Source_view) -> utl::Wrapper<hir::Pattern>;

    auto lower() noexcept {
        return [this](auto const& node) {
            return lower(node);
        };
    }

    [[noreturn]]
    auto error(utl::Source_view, utl::diagnostics::Message_arguments) const -> void;
};