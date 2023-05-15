#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <compiler/compiler.hpp>
#include <libparse/ast.hpp>
#include <libdesugar/hir.hpp>


struct Desugaring_context {
    compiler::Compilation_info compilation_info;
    hir::Node_arena            node_arena;
    compiler::Identifier       self_variable_identifier = compilation_info.get()->identifier_pool.make("self");

    explicit Desugaring_context(
        compiler::Compilation_info&& compilation_info,
        hir::Node_arena           && node_arena) noexcept
        : compilation_info { std::move(compilation_info) }
        , node_arena       { std::move(node_arena) } {}

    template <hir::node Node>
    auto wrap(Node&& node) -> utl::Wrapper<Node> {
        return node_arena.wrap(std::move(node));
    }
    [[nodiscard]]
    auto wrap() noexcept {
        return [this]<hir::node Node>(Node&& node) -> utl::Wrapper<Node> {
            return wrap(std::move(node));
        };
    }

    auto desugar(ast::Expression const&) -> hir::Expression;
    auto desugar(ast::Type       const&) -> hir::Type;
    auto desugar(ast::Pattern    const&) -> hir::Pattern;
    auto desugar(ast::Definition const&) -> hir::Definition;

    auto desugar(ast::Function_argument           const&) -> hir::Function_argument;
    auto desugar(ast::Function_parameter          const&) -> hir::Function_parameter;
    auto desugar(ast::Self_parameter              const&) -> hir::Function_parameter;
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
        return wrap(desugar(*node));
    }
    auto desugar() noexcept {
        return [this](auto const& node) { return desugar(node); };
    }

    auto unit_value      (utl::Source_view) -> utl::Wrapper<hir::Expression>;
    auto wildcard_pattern(utl::Source_view) -> utl::Wrapper<hir::Pattern>;
    auto true_pattern    (utl::Source_view) -> utl::Wrapper<hir::Pattern>;
    auto false_pattern   (utl::Source_view) -> utl::Wrapper<hir::Pattern>;

    [[noreturn]]
    auto error(utl::Source_view, utl::diagnostics::Message_arguments) -> void;
};
