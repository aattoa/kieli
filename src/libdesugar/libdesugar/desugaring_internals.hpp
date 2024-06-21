#pragma once

#include <libutl/utilities.hpp>
#include <libutl/safe_integer.hpp>
#include <libphase/phase.hpp>
#include <libparse/cst.hpp>
#include <libdesugar/ast.hpp>

namespace libdesugar {

    auto unit_type(utl::Source_range range) -> ast::Type;
    auto unit_value(utl::Source_range range) -> ast::Expression;
    auto wildcard_pattern(utl::Source_range range) -> ast::Pattern;

    struct Context {
        kieli::Compile_info& compile_info;
        ast::Node_arena&     node_arena;
        utl::Source_id       source;
        kieli::Identifier    self_variable_identifier
            = kieli::Identifier { compile_info.identifier_pool.make("self") };

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
        auto desugar(cst::expression::Struct_initializer::Field const&)
            -> ast::expression::Struct_initializer::Field;
        auto desugar(cst::pattern::Field const&) -> ast::pattern::Field;
        auto desugar(cst::pattern::Constructor_body const&) -> ast::pattern::Constructor_body;
        auto desugar(cst::definition::Field const&) -> ast::definition::Field;
        auto desugar(cst::definition::Constructor_body const&) -> ast::definition::Constructor_body;
        auto desugar(cst::definition::Constructor const&) -> ast::definition::Constructor;

        static auto desugar(cst::Wildcard const&) -> ast::Wildcard;
        static auto desugar(cst::Self_parameter const&) -> ast::Self_parameter;
        static auto desugar(cst::Mutability const&) -> ast::Mutability;

        auto desugar(cst::Type_annotation const&) -> ast::Type;

        auto desugar(utl::wrapper auto const node)
        {
            return wrap(desugar(*node));
        }

        auto desugar() noexcept
        {
            return [this](auto const& node) { return this->desugar(node); };
        }

        auto wrap_desugar()
        {
            return [this](auto const& node) { return wrap(this->desugar(node)); };
        }

        template <class T>
        auto desugar(std::vector<T> const& vector)
        {
            return vector | std::views::transform(desugar()) | std::ranges::to<std::vector>();
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

        auto desugar(cst::Type_parameter_default_argument const& argument)
        {
            return std::visit<ast::Template_type_parameter::Default>(desugar(), argument.variant);
        }

        auto desugar(cst::Value_parameter_default_argument const& argument)
        {
            return std::visit<ast::Template_value_parameter::Default>(desugar(), argument.variant);
        }

        auto desugar(cst::Mutability_parameter_default_argument const& default_argument)
        {
            return std::visit<ast::Template_mutability_parameter::Default>(
                desugar(), default_argument.variant);
        }

        auto deref_desugar()
        {
            return [this](utl::wrapper auto const node) { return this->desugar(*node); };
        }

        auto        desugar_mutability(cst::Mutability const&, utl::Source_range) = delete;
        static auto desugar_mutability(std::optional<cst::Mutability> const&, utl::Source_range)
            -> ast::Mutability;

        auto normalize_self_parameter(cst::Self_parameter const&) -> ast::Function_parameter;
    };
} // namespace libdesugar
