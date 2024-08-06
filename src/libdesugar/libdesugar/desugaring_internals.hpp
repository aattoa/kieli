#pragma once

#include <libutl/utilities.hpp>
#include <libutl/safe_integer.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

namespace libdesugar {
    namespace cst = kieli::cst;
    namespace ast = kieli::ast;

    auto unit_type(kieli::Range range) -> ast::Type;
    auto unit_value(kieli::Range range) -> ast::Expression;
    auto wildcard_pattern(kieli::Range range) -> ast::Pattern;

    struct Context {
        kieli::Database&  db;
        cst::Arena const& cst;
        ast::Arena&       ast;
        kieli::Source_id  source;
        kieli::Identifier self_variable_identifier
            = kieli::Identifier { db.string_pool.add("self") };

        auto desugar(cst::Expression const&) -> ast::Expression;
        auto desugar(cst::Type const&) -> ast::Type;
        auto desugar(cst::Pattern const&) -> ast::Pattern;
        auto desugar(cst::Definition const&) -> ast::Definition;

        auto desugar(cst::Function_argument const&) -> ast::Function_argument;
        auto desugar(cst::Function_parameter const&) -> ast::Function_parameter;
        auto desugar(cst::Template_argument const&) -> ast::Template_argument;
        auto desugar(cst::Template_parameter const&) -> ast::Template_parameter;
        auto desugar(cst::Path_segment const&) -> ast::Path_segment;
        auto desugar(cst::Path const&) -> ast::Path;
        auto desugar(cst::Concept_reference const&) -> ast::Concept_reference;
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

        // auto desugar(cst::Type_annotation const&) -> ast::Type;
        auto desugar(cst::Type_annotation const&) -> ast::Type_id;

        auto desugar() noexcept
        {
            return [this](auto const& node) { return this->desugar(node); };
        }

        auto desugar(cst::Expression_id) -> ast::Expression_id;
        auto desugar(cst::Pattern_id) -> ast::Pattern_id;
        auto desugar(cst::Type_id) -> ast::Type_id;

        auto wrap_desugar(cst::Expression const&) -> ast::Expression_id;
        auto wrap_desugar(cst::Pattern const&) -> ast::Pattern_id;
        auto wrap_desugar(cst::Type const&) -> ast::Type_id;

        auto deref_desugar(cst::Expression_id) -> ast::Expression;
        auto deref_desugar(cst::Pattern_id) -> ast::Pattern;
        auto deref_desugar(cst::Type_id) -> ast::Type;

        auto wrap_desugar() noexcept
        {
            return [this](auto const& node) { return this->wrap_desugar(node); };
        }

        auto deref_desugar() noexcept
        {
            return [this](auto const& node) { return this->deref_desugar(node); };
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

        auto desugar_mutability(cst::Mutability const&, kieli::Range) = delete;

        static auto desugar_mutability(std::optional<cst::Mutability> const&, kieli::Range)
            -> ast::Mutability;

        auto normalize_self_parameter(cst::Self_parameter const&) -> ast::Function_parameter;
    };
} // namespace libdesugar
