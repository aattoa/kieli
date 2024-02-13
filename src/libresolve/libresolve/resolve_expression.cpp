#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace hir = libresolve::hir;

namespace {
    struct Expression_resolution_visitor {
        libresolve::Context&            context;
        libresolve::Unification_state&  state;
        libresolve::Scope&              scope;
        libresolve::Environment_wrapper environment;
        ast::Expression const&          this_expression;

        auto recurse()
        {
            return [&](ast::Expression const& expression) -> hir::Expression {
                return libresolve::resolve_expression(
                    context, state, scope, environment, expression);
            };
        }

        auto recurse(ast::Expression const& expression) -> hir::Expression
        {
            return recurse()(expression);
        }

        auto operator()(kieli::Integer const& integer) -> hir::Expression
        {
            return {
                .variant = integer,
                .type {
                    context.arenas.type(hir::type::Unification_variable {
                        state.fresh_integral_type_variable(),
                    }),
                    this_expression.source_range,
                },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Expression
        {
            return {
                .variant      = floating,
                .type         = { context.constants.floating_type, this_expression.source_range },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(kieli::Character const& character) -> hir::Expression
        {
            return {
                .variant      = character,
                .type         = { context.constants.character_type, this_expression.source_range },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Expression
        {
            return {
                .variant      = boolean,
                .type         = { context.constants.boolean_type, this_expression.source_range },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(kieli::String const& string) -> hir::Expression
        {
            return {
                .variant      = string,
                .type         = { context.constants.string_type, this_expression.source_range },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(ast::expression::Array_literal const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Self const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Variable const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression
        {
            std::vector<hir::Expression> fields
                = std::ranges::to<std::vector>(std::views::transform(tuple.fields, recurse()));

            std::vector<hir::Type> types = std::ranges::to<std::vector>(
                std::views::transform(fields, &hir::Expression::type));

            return {
                .variant = hir::expression::Tuple { .fields = std::move(fields) },
                .type {
                    context.arenas.type(hir::type::Tuple { .types = std::move(types) }),
                    this_expression.source_range,
                },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(ast::expression::Loop const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Break const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Continue const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Block const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Invocation const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Unit_initializer const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Tuple_initializer const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Struct_initializer const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Binary_operator_invocation const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Struct_field_access const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Tuple_field_access const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Array_index_access const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Method_invocation const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Conditional const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Match const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Template_application const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Type_cast const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Type_ascription const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Let_binding const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Local_type_alias const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Ret const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression
        {
            return {
                .variant = hir::expression::Sizeof {
                    libresolve::resolve_type(context, state, scope, environment, *sizeof_.inspected_type),
                },
                .type {
                    .variant = context.arenas.type(
                        hir::type::Unification_variable { state.fresh_integral_type_variable() }),
                    .source_range = this_expression.source_range,
                },
                .source_range = this_expression.source_range,
            };
        }

        auto operator()(ast::expression::Reference const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Addressof const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Reference_dereference const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Pointer_dereference const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Unsafe const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Move const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Meta const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Hole const&) -> hir::Expression
        {
            cpputil::todo();
        }
    };
} // namespace

auto libresolve::resolve_expression(
    Context&               context,
    Unification_state&     state,
    Scope&                 scope,
    Environment_wrapper    environment,
    ast::Expression const& expression) -> hir::Expression
{
    return std::visit(
        Expression_resolution_visitor { context, state, scope, environment, expression },
        expression.value);
}
