#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <libphase/phase.hpp>
#include <libparse/cst.hpp>
#include <libdesugar/ast.hpp>

namespace libdesugar {
    struct Context {
        kieli::Compile_info& compile_info;
        ast::Node_arena&     node_arena;
        kieli::Identifier    self_variable_identifier
            = kieli::Identifier { compile_info.identifier_pool.make("self") };

        explicit Context(kieli::Compile_info& compile_info, ast::Node_arena& node_arena) noexcept
            : compile_info { compile_info }
            , node_arena { node_arena }
        {}

        template <ast::node Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node>
        {
            return node_arena.wrap<Node>(static_cast<Node&&>(node));
        }

        [[nodiscard]] auto wrap() noexcept
        {
            return [this]<ast::node Node>(Node&& node) -> utl::Wrapper<Node> {
                return wrap(static_cast<Node&&>(node));
            };
        }

        auto desugar(cst::Expression const&) -> ast::Expression;
        auto desugar(cst::Type const&) -> ast::Type;
        auto desugar(cst::Pattern const&) -> ast::Pattern;
        auto desugar(cst::Definition const&) -> ast::Definition;

        auto desugar(cst::Function_argument const&) -> ast::Function_argument;
        auto desugar(cst::Function_parameter const&) -> ast::Function_parameter;
        auto desugar(cst::Template_argument const&) -> ast::Template_argument;
        auto desugar(cst::Template_parameter const&) -> ast::Template_parameter;
        auto desugar(cst::Qualifier const&) -> ast::Qualifier;
        auto desugar(cst::Qualified_name const&) -> ast::Qualified_name;
        auto desugar(cst::Class_reference const&) -> ast::Class_reference;
        auto desugar(cst::Function_signature const&) -> ast::Function_signature;
        auto desugar(cst::Type_signature const&) -> ast::Type_signature;

        static auto desugar(cst::Self_parameter const&) -> ast::Self_parameter;
        static auto desugar(cst::Mutability const&) -> ast::Mutability;

        auto desugar(cst::Type_annotation const&) -> ast::Type;
        auto desugar(cst::Function_parameter::Default_argument const&)
            -> utl::Wrapper<ast::Expression>;
        auto desugar(cst::Template_parameter::Default_argument const&) -> ast::Template_argument;

        auto desugar(utl::wrapper auto const node)
        {
            return wrap(desugar(*node));
        }

        auto desugar() noexcept
        {
            return [this](auto const& node) { return this->desugar(node); };
        }

        template <class T>
        auto desugar(std::vector<T> const& vector)
        {
            return std::ranges::to<std::vector>(std::views::transform(vector, desugar()));
        }

        template <class T>
        auto desugar(cst::Separated_sequence<T> const& sequence)
        {
            return desugar(sequence.elements);
        }

        template <class T>
        auto desugar(cst::Surrounded<T> const& surrounded)
        {
            return desugar(surrounded.value);
        }

        auto deref_desugar()
        {
            return [this](utl::wrapper auto const node) { return this->desugar(*node); };
        }

        auto wrap_desugar()
        {
            return [this](auto const& node) { return wrap(this->desugar(node)); };
        }

        static auto desugar_mutability(std::optional<cst::Mutability> const&, utl::Source_view)
            -> ast::Mutability;

        auto desugar_mutability(cst::Mutability const&, utl::Source_view) = delete;

        auto normalize_self_parameter(cst::Self_parameter const&) -> ast::Function_parameter;

        auto unit_value(utl::Source_view) -> utl::Wrapper<ast::Expression>;
        auto wildcard_pattern(utl::Source_view) -> utl::Wrapper<ast::Pattern>;
        auto true_pattern(utl::Source_view) -> utl::Wrapper<ast::Pattern>;
        auto false_pattern(utl::Source_view) -> utl::Wrapper<ast::Pattern>;

        [[nodiscard]] auto diagnostics() noexcept -> kieli::Diagnostics&;
    };
} // namespace libdesugar
