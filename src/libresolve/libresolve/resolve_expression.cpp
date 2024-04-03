#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    struct Expression_resolution_visitor {
        Context&               context;
        Inference_state&       state;
        Scope&                 scope;
        Environment_wrapper    environment;
        ast::Expression const& this_expression;

        template <class... Args>
        auto error(std::format_string<Args...> const fmt, Args&&... args) -> hir::Expression
        {
            context.compile_info.diagnostics.emit(
                cppdiag::Severity::error,
                environment->source,
                this_expression.source_range,
                fmt,
                std::forward<Args>(args)...);
            return error_expression(context.constants, this_expression.source_range);
        }

        auto recurse()
        {
            return [&](ast::Expression const& expression) -> hir::Expression {
                return resolve_expression(context, state, scope, environment, expression);
            };
        }

        auto recurse(ast::Expression const& expression) -> hir::Expression
        {
            return recurse()(expression);
        }

        auto operator()(kieli::Integer const& integer) -> hir::Expression
        {
            return {
                integer,
                state.fresh_integral_type_variable(context.arenas, this_expression.source_range),
                this_expression.source_range,
            };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Expression
        {
            return {
                floating,
                hir::Type { context.constants.floating_type, this_expression.source_range },
                this_expression.source_range,
            };
        }

        auto operator()(kieli::Character const& character) -> hir::Expression
        {
            return {
                character,
                hir::Type { context.constants.character_type, this_expression.source_range },
                this_expression.source_range,
            };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Expression
        {
            return {
                boolean,
                hir::Type { context.constants.boolean_type, this_expression.source_range },
                this_expression.source_range,
            };
        }

        auto operator()(kieli::String const& string) -> hir::Expression
        {
            return {
                string,
                hir::Type { context.constants.string_type, this_expression.source_range },
                this_expression.source_range,
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

        auto operator()(ast::expression::Variable const& variable) -> hir::Expression
        {
            if (variable.name.is_unqualified()) {
                if (Variable_bind const* const bind
                    = scope.find_variable(variable.name.primary_name.identifier))
                {
                    return hir::Expression {
                        hir::expression::Variable_reference {
                            .tag        = bind->tag,
                            .identifier = bind->name.identifier,
                        },
                        bind->type,
                        this_expression.source_range,
                    };
                }
            }
            if (auto const lookup_result
                = lookup_lower(context, state, scope, environment, variable.name))
            {
                return std::visit(
                    utl::Overload {
                        [&](utl::Mutable_wrapper<Function_info> const function) {
                            auto& signature
                                = resolve_function_signature(context, function.as_mutable());
                            return hir::Expression {
                                hir::expression::Function_reference { .info = function },
                                signature.function_type,
                                this_expression.source_range,
                            };
                        },
                        [&](utl::Mutable_wrapper<Module_info> const module) {
                            return error(
                                "Expected an expression, but found a reference to module '{}'",
                                module->name);
                        },
                    },
                    lookup_result.value().variant);
            }
            return error("Use of an undeclared identifier");
        }

        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression
        {
            auto fields = tuple.fields                     //
                        | std::views::transform(recurse()) //
                        | std::ranges::to<std::vector>();

            auto types = fields //
                       | std::views::transform(&hir::Expression::type)
                       | std::ranges::to<std::vector>();

            return {
                hir::expression::Tuple { .fields = std::move(fields) },
                hir::Type {
                    context.arenas.type(hir::type::Tuple { .types = std::move(types) }),
                    this_expression.source_range,
                },
                this_expression.source_range,
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

        auto operator()(ast::expression::Block const& block) -> hir::Expression
        {
            Scope block_scope = scope.child();

            auto const resolve_effect = [&](ast::Expression const& expression) {
                hir::Expression effect
                    = resolve_expression(context, state, block_scope, environment, expression);
                require_subtype_relationship( // TODO: better error message
                    context.compile_info.diagnostics,
                    state,
                    effect.type,
                    hir::Type { context.constants.unit_type, effect.source_range },
                    effect.source_range);
                return effect;
            };

            auto side_effects = block.side_effects                    //
                              | std::views::transform(resolve_effect) //
                              | std::ranges::to<std::vector>();

            hir::Expression result
                = resolve_expression(context, state, scope, environment, *block.result);
            hir::Type const result_type = result.type;

            return {
                hir::expression::Block {
                    .side_effects = std::move(side_effects),
                    .result       = context.arenas.wrap(std::move(result)),
                },
                result_type,
                this_expression.source_range,
            };
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

        auto operator()(ast::expression::Local_type_alias const& alias) -> hir::Expression
        {
            scope.bind_type(
                alias.name.identifier,
                Type_bind {
                    .name = alias.name,
                    .type = resolve_type(context, state, scope, environment, *alias.aliased_type),
                });
            return unit_expression(context.constants, this_expression.source_range);
        }

        auto operator()(ast::expression::Ret const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression
        {
            return {
                hir::expression::Sizeof {
                    resolve_type(context, state, scope, environment, *sizeof_.inspected_type),
                },
                state.fresh_integral_type_variable(context.arenas, this_expression.source_range),
                this_expression.source_range,
            };
        }

        auto operator()(ast::expression::Addressof const& addressof) -> hir::Expression
        {
            hir::Expression lvalue_expression = recurse(*addressof.lvalue_expression);
            hir::Type const referenced_type   = lvalue_expression.type;
            hir::Mutability mutability
                = resolve_mutability(context, state, scope, addressof.mutability);
            return {
                hir::expression::Addressof {
                    .mutability        = mutability,
                    .lvalue_expression = context.arenas.wrap(std::move(lvalue_expression)),
                },
                hir::Type {
                    context.arenas.type(hir::type::Reference {
                        .referenced_type = referenced_type,
                        .mutability      = mutability,
                    }),
                    this_expression.source_range,
                },
                this_expression.source_range,
            };
        }

        auto operator()(ast::expression::Dereference const& dereference) -> hir::Expression
        {
            hir::Type const referenced_type
                = state.fresh_general_type_variable(context.arenas, this_expression.source_range);
            hir::Mutability const reference_mutability
                = state.fresh_mutability_variable(context.arenas, this_expression.source_range);

            hir::Type const reference_type {
                context.arenas.type(hir::type::Reference {
                    .referenced_type = referenced_type,
                    .mutability      = reference_mutability,
                }),
                this_expression.source_range,
            };
            hir::Expression reference_expression = recurse(*dereference.reference_expression);

            require_subtype_relationship(
                context.compile_info.diagnostics,
                state,
                reference_expression.type,
                reference_type,
                this_expression.source_range);

            return {
                hir::expression::Dereference {
                    .reference_expression = context.arenas.wrap(std::move(reference_expression)),
                },
                referenced_type,
                this_expression.source_range,
            };
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
            return {
                hir::expression::Hole {},
                state.fresh_general_type_variable(context.arenas, this_expression.source_range),
                this_expression.source_range,
            };
        }
    };
} // namespace

auto libresolve::resolve_expression(
    Context&               context,
    Inference_state&       state,
    Scope&                 scope,
    Environment_wrapper    environment,
    ast::Expression const& expression) -> hir::Expression
{
    return std::visit(
        Expression_resolution_visitor { context, state, scope, environment, expression },
        expression.variant);
}
