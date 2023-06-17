#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>
#include <libparse/cst.hpp>
#include <libdesugar/hir.hpp>


namespace libdesugar {
    struct Desugar_context {
        compiler::Compilation_info compilation_info;
        hir::Node_arena            node_arena;
        compiler::Identifier       self_variable_identifier = compilation_info.get()->identifier_pool.make("self");

        explicit Desugar_context(
            compiler::Compilation_info&& compilation_info,
            hir::Node_arena&& node_arena) noexcept
            : compilation_info { std::move(compilation_info) }, node_arena { std::move(node_arena) } {}

        template<hir::node Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node> {
            return node_arena.wrap(std::move(node));
        }
        [[nodiscard]]
        auto wrap() noexcept {
            return [this]<hir::node Node>(Node&& node) -> utl::Wrapper<Node> {
                return wrap(std::move(node));
            };
        }

        auto desugar(cst::Expression const&) -> hir::Expression;
        auto desugar(cst::Type       const&) -> hir::Type;
        auto desugar(cst::Pattern    const&) -> hir::Pattern;
        auto desugar(cst::Definition const&) -> hir::Definition;

        auto desugar(cst::Function_argument  const&) -> hir::Function_argument;
        auto desugar(cst::Function_parameter const&) -> hir::Function_parameter;
        auto desugar(cst::Self_parameter     const&) -> hir::Self_parameter;
        auto desugar(cst::Template_argument  const&) -> hir::Template_argument;
        auto desugar(cst::Template_parameter const&) -> hir::Template_parameter;
        auto desugar(cst::Qualifier          const&) -> hir::Qualifier;
        auto desugar(cst::Qualified_name     const&) -> hir::Qualified_name;
        auto desugar(cst::Class_reference    const&) -> hir::Class_reference;
        auto desugar(cst::Function_signature const&) -> hir::Function_signature;
        auto desugar(cst::Type_signature     const&) -> hir::Type_signature;
        auto desugar(cst::Mutability         const&) -> hir::Mutability;

        auto desugar(cst::Type_annotation                      const&) -> hir::Type;
        auto desugar(cst::Function_parameter::Default_argument const&) -> utl::Wrapper<hir::Expression>;
        auto desugar(cst::Template_parameter::Default_argument const&) -> hir::Template_argument;

        auto desugar(utl::wrapper auto const node) {
            return wrap(desugar(*node));
        }
        auto desugar() noexcept {
            return [this](auto const& node) { return desugar(node); };
        }

        auto deref_desugar() {
            return [this](utl::wrapper auto const node) {
                return desugar(*node);
            };
        }

        auto desugar_mutability(tl::optional<cst::Mutability> const&, utl::Source_view) -> hir::Mutability;
        auto desugar_mutability(cst::Mutability const&, utl::Source_view) = delete;

        auto normalize_self_parameter(cst::Self_parameter const&) -> hir::Function_parameter;

        auto unit_value      (utl::Source_view) -> utl::Wrapper<hir::Expression>;
        auto wildcard_pattern(utl::Source_view) -> utl::Wrapper<hir::Pattern>;
        auto true_pattern    (utl::Source_view) -> utl::Wrapper<hir::Pattern>;
        auto false_pattern   (utl::Source_view) -> utl::Wrapper<hir::Pattern>;

        [[noreturn]] auto error(utl::Source_view, utl::diagnostics::Message_arguments) -> void;
    };
}
