#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace libresolve;

namespace {
    struct Expression_resolution_visitor {
        Context&               context;
        Inference_state&       state;
        hir::Scope_id          scope_id;
        hir::Environment_id    environment_id;
        ast::Expression const& this_expression;

        auto ast() -> ast::Arena&
        {
            return context.documents.at(state.document_id).ast;
        }

        auto error(kieli::Range const range, std::string message) -> hir::Expression
        {
            kieli::add_error(context.db, state.document_id, range, std::move(message));
            return error_expression(context.constants, this_expression.range);
        }

        auto unsupported() -> hir::Expression
        {
            return error(this_expression.range, "Unsupported expression");
        }

        auto recurse()
        {
            return [&](ast::Expression const& expression) -> hir::Expression {
                return resolve_expression(context, state, scope_id, environment_id, expression);
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
                fresh_integral_type_variable(state, context.hir, this_expression.range).id,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Expression
        {
            return {
                floating,
                context.constants.floating_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::Character const& character) -> hir::Expression
        {
            return {
                character,
                context.constants.character_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Expression
        {
            return {
                boolean,
                context.constants.boolean_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::String const& string) -> hir::Expression
        {
            return {
                string,
                context.constants.string_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::Path const&) -> hir::Expression
        {
            return error(this_expression.range, "Path expressions are not supported yet");
        }

        auto operator()(ast::expression::Array const& array) -> hir::Expression
        {
            hir::Type const element_type
                = fresh_general_type_variable(state, context.hir, this_expression.range);

            std::vector<hir::Expression> elements;
            for (ast::Expression const& element : array.elements) {
                elements.push_back(recurse(element));
                require_subtype_relationship(
                    context,
                    state,
                    element.range,
                    context.hir.types[elements.back().type],
                    context.hir.types[element_type.id]);
            }

            return {
                hir::expression::Array_literal { std::move(elements) },
                context.hir.types.push(hir::type::Array {
                    .element_type = element_type,
                    .length       = context.hir.expressions.push(hir::Expression {
                        kieli::Integer { elements.size() },
                        context.constants.u64_type,
                        hir::Expression_kind::value,
                        this_expression.range,
                    }),
                }),
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression
        {
            auto fields = std::views::transform(tuple.fields, recurse()) //
                        | std::ranges::to<std::vector>();
            auto types = std::views::transform(fields, hir::expression_type)
                       | std::ranges::to<std::vector>();
            return {
                hir::expression::Tuple { .fields = std::move(fields) },
                context.hir.types.push(hir::type::Tuple { std::move(types) }),
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Loop const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Break const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Continue const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Block const& block) -> hir::Expression
        {
            return child_scope(context, scope_id, [&](hir::Scope_id block_scope_id) {
                auto const resolve_effect = [&](ast::Expression const& expression) {
                    hir::Expression effect = resolve_expression(
                        context, state, block_scope_id, environment_id, expression);
                    require_subtype_relationship(
                        context,
                        state,
                        effect.range,
                        context.hir.types[effect.type],
                        context.hir.types[context.constants.unit_type]);
                    return effect;
                };

                auto side_effects = std::views::transform(block.side_effects, resolve_effect)
                                  | std::ranges::to<std::vector>();

                hir::Expression result = resolve_expression(
                    context, state, scope_id, environment_id, ast().expressions[block.result]);

                auto const result_type = result.type;

                return hir::Expression {
                    hir::expression::Block {
                        .side_effects = std::move(side_effects),
                        .result       = context.hir.expressions.push(std::move(result)),
                    },
                    result_type,
                    hir::Expression_kind::value,
                    this_expression.range,
                };
            });
        }

        auto operator()(ast::expression::Function_call const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Tuple_initializer const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Struct_initializer const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Infix_call const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Struct_field const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Tuple_field const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Array_index const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Method_call const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Conditional const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Match const& match) -> hir::Expression
        {
            ast::Arena& ast = this->ast();

            hir::Expression matched_expression = recurse(ast.expressions[match.expression]);
            hir::Type const result_type
                = fresh_general_type_variable(state, context.hir, this_expression.range);

            auto const resolve_case = [&](ast::Match_case const& match_case) {
                hir::Pattern pattern = resolve_pattern(
                    context, state, scope_id, environment_id, ast.patterns[match_case.pattern]);
                require_subtype_relationship(
                    context,
                    state,
                    matched_expression.range,
                    context.hir.types[matched_expression.type],
                    context.hir.types[pattern.type]);
                hir::Expression expression = recurse(ast.expressions[match_case.expression]);
                require_subtype_relationship(
                    context,
                    state,
                    expression.range,
                    context.hir.types[expression.type],
                    context.hir.types[result_type.id]);
                return hir::Match_case {
                    .pattern    = context.hir.patterns.push(std::move(pattern)),
                    .expression = context.hir.expressions.push(std::move(expression)),
                };
            };

            return {
                hir::expression::Match {
                    .cases = std::views::transform(match.cases, resolve_case)
                           | std::ranges::to<std::vector>(),
                    .expression = context.hir.expressions.push(std::move(matched_expression)),
                },
                result_type.id,
                hir::Expression_kind::value, // TODO
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Type_ascription const& ascription) -> hir::Expression
        {
            ast::Arena& ast = this->ast();

            hir::Expression expression = recurse(ast.expressions[ascription.expression]);
            hir::Type const ascribed   = resolve_type(
                context, state, scope_id, environment_id, ast.types[ascription.ascribed_type]);
            require_subtype_relationship(
                context,
                state,
                expression.range,
                context.hir.types[expression.type],
                context.hir.types[ascribed.id]);
            return expression;
        }

        auto operator()(ast::expression::Let const& let) -> hir::Expression
        {
            ast::Arena&  ast     = this->ast();
            hir::Pattern pattern = resolve_pattern(
                context, state, scope_id, environment_id, ast.patterns[let.pattern]);
            hir::Type type
                = resolve_type(context, state, scope_id, environment_id, ast.types[let.type]);
            hir::Expression initializer = recurse(ast.expressions[let.initializer]);

            require_subtype_relationship(
                context,
                state,
                pattern.range,
                context.hir.types[pattern.type],
                context.hir.types[type.id]);
            require_subtype_relationship(
                context,
                state,
                initializer.range,
                context.hir.types[initializer.type],
                context.hir.types[type.id]);

            return {
                hir::expression::Let {
                    .pattern     = context.hir.patterns.push(std::move(pattern)),
                    .type        = type,
                    .initializer = context.hir.expressions.push(std::move(initializer)),
                },
                context.constants.unit_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Type_alias const& alias) -> hir::Expression
        {
            hir::Type const type
                = resolve_type(context, state, scope_id, environment_id, ast().types[alias.type]);
            bind_type(
                context.info.scopes.index_vector[scope_id],
                alias.name.identifier,
                Type_bind { .name = alias.name, .type = type.id });
            return unit_expression(context.constants, this_expression.range);
        }

        auto operator()(ast::expression::Ret const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression
        {
            return {
                hir::expression::Sizeof {
                    resolve_type(
                        context,
                        state,
                        scope_id,
                        environment_id,
                        ast().types[sizeof_.inspected_type]),
                },
                fresh_integral_type_variable(state, context.hir, this_expression.range).id,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Addressof const& addressof) -> hir::Expression
        {
            hir::Expression place_expression
                = recurse(ast().expressions[addressof.place_expression]);
            hir::Type_id const referenced_type = place_expression.type;
            hir::Mutability    mutability
                = resolve_mutability(context, scope_id, addressof.mutability);
            if (place_expression.kind != hir::Expression_kind::place) {
                return error(
                    place_expression.range,
                    "This expression does not identify a place in memory, "
                    "so its address can not be taken");
            }
            return {
                hir::expression::Addressof {
                    .mutability       = mutability,
                    .place_expression = context.hir.expressions.push(std::move(place_expression)),
                },
                context.hir.types.push(hir::type::Reference {
                    .referenced_type { referenced_type, this_expression.range },
                    .mutability = mutability,
                }),
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Dereference const& dereference) -> hir::Expression
        {
            hir::Type const referenced_type
                = fresh_general_type_variable(state, context.hir, this_expression.range);
            hir::Mutability const reference_mutability
                = fresh_mutability_variable(state, context.hir, this_expression.range);
            hir::Type const reference_type {
                context.hir.types.push(hir::type::Reference {
                    .referenced_type = referenced_type,
                    .mutability      = reference_mutability,
                }),
                this_expression.range,
            };

            hir::Expression reference_expression
                = recurse(ast().expressions[dereference.reference_expression]);
            require_subtype_relationship(
                context,
                state,
                reference_expression.range,
                context.hir.types[reference_expression.type],
                context.hir.types[reference_type.id]);

            return {
                hir::expression::Dereference {
                    context.hir.expressions.push(std::move(reference_expression)),
                },
                referenced_type.id,
                hir::Expression_kind::place,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Defer const& defer) -> hir::Expression
        {
            return {
                hir::expression::Defer {
                    context.hir.expressions.push(
                        recurse(ast().expressions[defer.effect_expression])),
                },
                context.constants.unit_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Move const&) -> hir::Expression
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::Wildcard const&) -> hir::Expression
        {
            return {
                hir::Wildcard {},
                fresh_general_type_variable(state, context.hir, this_expression.range).id,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::Error const&) -> hir::Expression
        {
            return error_expression(context.constants, this_expression.range);
        }
    };
} // namespace

auto libresolve::resolve_expression(
    Context&                  context,
    Inference_state&          state,
    hir::Scope_id const       scope_id,
    hir::Environment_id const environment_id,
    ast::Expression const&    expression) -> hir::Expression
{
    Expression_resolution_visitor visitor { context, state, scope_id, environment_id, expression };
    return std::visit(visitor, expression.variant);
}
