#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    auto constant_loop_condition_diagnostic(kieli::Range const range, bool const condition)
        -> kieli::Diagnostic
    {
        return kieli::Diagnostic {
            .message  = condition ? "Use `loop` instead of `while true`" : "Loop will never run",
            .range    = range,
            .severity = condition ? kieli::Severity::information : kieli::Severity::warning,
        };
    }

    auto duplicate_fields_error(
        Context const           context,
        kieli::Identifier const identifier,
        kieli::Range const      first,
        kieli::Range const      second) -> kieli::Diagnostic
    {
        return kieli::Diagnostic {
            .message      = std::format("Duplicate initializer for struct member {}", identifier),
            .range        = second,
            .severity     = kieli::Severity::error,
            .related_info = utl::to_vector({
                kieli::Diagnostic_related {
                    .message = "First specified here",
                    .location { .document_id = context.document_id, .range = first },
                },
            }),
        };
    }

    auto check_has_duplicate_fields(
        Context const context, cst::expression::Struct_initializer const& initializer) -> bool
    {
        auto const& initializers = initializer.initializers.value.elements;
        for (auto it = initializers.begin(); it != initializers.end(); ++it) {
            auto const duplicate = std::ranges::find(
                it + 1, initializers.end(), it->name, &cst::Struct_field_initializer::name);
            if (duplicate != initializers.end()) {
                kieli::Diagnostic diagnostic = duplicate_fields_error(
                    context, it->name.identifier, it->name.range, duplicate->name.range);
                kieli::add_diagnostic(context.db, context.document_id, std::move(diagnostic));
                return true;
            }
        }
        return false;
    }

    auto break_expression(Context const context, kieli::Range const range) -> ast::Expression_id
    {
        return context.ast.expressions.push(
            ast::expression::Break { context.ast.expressions.push(unit_value(range)) }, range);
    }

    struct Expression_desugaring_visitor {
        Context                context;
        cst::Expression const& this_expression;

        auto operator()(kieli::literal auto const& literal) const -> ast::Expression_variant
        {
            return literal;
        }

        auto operator()(cst::Path const& path) const -> ast::Expression_variant
        {
            return desugar(context, path);
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Expression_variant
        {
            return ast::Wildcard { .range = context.cst.tokens[wildcard.underscore_token].range };
        }

        auto operator()(cst::expression::Paren const& paren) const -> ast::Expression_variant
        {
            cst::Expression const& expr = context.cst.expressions[paren.expression.value];
            return std::visit(Expression_desugaring_visitor { context, expr }, expr.variant);
        }

        auto operator()(cst::expression::Array const& literal) const -> ast::Expression_variant
        {
            return ast::expression::Array { std::ranges::to<std::vector>(
                std::views::transform(literal.elements.value.elements, deref_desugar(context))) };
        }

        auto operator()(cst::expression::Tuple const& tuple) const -> ast::Expression_variant
        {
            return ast::expression::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.fields.value.elements, deref_desugar(context))) };
        }

        auto operator()(cst::expression::Conditional const& conditional) const
            -> ast::Expression_variant
        {
            ast::Expression_id const false_branch
                = conditional.false_branch.has_value()
                    ? desugar(context, conditional.false_branch->body)
                    : context.ast.expressions.push(unit_value(this_expression.range));

            if (auto const* const let = std::get_if<cst::expression::Conditional_let>(
                    &context.cst.expressions[conditional.condition].variant)) {
                /*
                    if let a = b { c } else { d }

                    is transformed into

                    match b {
                        a -> c
                        _ -> d
                    }
                */
                return ast::expression::Match {
                    .cases = utl::to_vector({
                        ast::Match_case {
                            .pattern    = desugar(context, let->pattern),
                            .expression = desugar(context, conditional.true_branch),
                        },
                        ast::Match_case {
                            .pattern = context.ast.patterns.push(
                                wildcard_pattern(context.cst.patterns[let->pattern].range)),
                            .expression = false_branch,
                        },
                    }),

                    .expression = desugar(context, let->initializer),
                };
            }

            ast::Expression condition = deref_desugar(context, conditional.condition);

            if (std::holds_alternative<kieli::Boolean>(condition.variant)) {
                kieli::Diagnostic diagnostic {
                    .message  = "Constant condition",
                    .range    = condition.range,
                    .severity = kieli::Severity::information,
                };
                kieli::add_diagnostic(context.db, context.document_id, std::move(diagnostic));
            }

            return ast::expression::Conditional {
                .condition                 = context.ast.expressions.push(std::move(condition)),
                .true_branch               = desugar(context, conditional.true_branch),
                .false_branch              = false_branch,
                .source                    = conditional.is_elif ? ast::Conditional_source::elif
                                                                 : ast::Conditional_source::if_,
                .has_explicit_false_branch = conditional.false_branch.has_value(),
            };
        }

        auto operator()(cst::expression::Match const& match) const -> ast::Expression_variant
        {
            auto const desugar_case = [&](cst::Match_case const& match_case) {
                return ast::Match_case {
                    .pattern    = desugar(context, match_case.pattern),
                    .expression = desugar(context, match_case.handler),
                };
            };
            return ast::expression::Match {
                .cases = std::views::transform(match.cases.value, desugar_case)
                       | std::ranges::to<std::vector>(),
                .expression = desugar(context, match.matched_expression),
            };
        }

        auto operator()(cst::expression::Block const& block) const -> ast::Expression_variant
        {
            auto desugar_effect
                = [&](auto const& effect) { return deref_desugar(context, effect.expression); };

            auto side_effects = std::views::transform(block.side_effects, desugar_effect)
                              | std::ranges::to<std::vector>();

            cst::Token_id const unit_token = //
                block.side_effects.empty()   //
                    ? block.close_brace_token
                    : block.side_effects.back().trailing_semicolon_token;

            ast::Expression_id const result = //
                block.result_expression.has_value()
                    ? desugar(context, block.result_expression.value())
                    : context.ast.expressions.push(
                          unit_value(context.cst.tokens[unit_token].range));

            return ast::expression::Block {
                .side_effects = std::move(side_effects),
                .result       = result,
            };
        }

        auto desugar_while_let_loop(
            cst::expression::While_loop const&      loop,
            cst::expression::Conditional_let const& let) const -> ast::Expression_variant
        {
            /*
                while let a = b { c }

                is transformed into

                loop {
                    match b {
                        a -> c
                        _ -> break
                    }
                }
            */
            ast::expression::Match match {
                .cases      = utl::to_vector({
                    ast::Match_case {
                        desugar(context, let.pattern),
                        desugar(context, loop.body),
                    },
                    ast::Match_case {
                        context.ast.patterns.push(wildcard_pattern(this_expression.range)),
                        break_expression(context, this_expression.range),
                    },
                }),
                .expression = desugar(context, let.initializer),
            };
            return ast::expression::Loop {
                .body = context.ast.expressions.push(
                    std::move(match), context.cst.expressions[loop.body].range),
                .source = ast::Loop_source::while_loop,
            };
        }

        auto desugar_while_loop(cst::expression::While_loop const& loop) const
            -> ast::Expression_variant
        {
            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */
            ast::Expression condition = deref_desugar(context, loop.condition);
            if (auto const* const boolean = std::get_if<kieli::Boolean>(&condition.variant)) {
                kieli::add_diagnostic(
                    context.db,
                    context.document_id,
                    constant_loop_condition_diagnostic(condition.range, boolean->value));
            }
            return ast::expression::Loop {
                .body = context.ast.expressions.push(
                    ast::expression::Conditional {
                        .condition    = context.ast.expressions.push(std::move(condition)),
                        .true_branch  = desugar(context, loop.body),
                        .false_branch = break_expression(context, this_expression.range),
                        .source       = ast::Conditional_source::while_loop_body,
                        .has_explicit_false_branch = true,
                    },
                    context.cst.expressions[loop.body].range),
                .source = ast::Loop_source::while_loop,
            };
        }

        auto operator()(cst::expression::While_loop const& loop) const -> ast::Expression_variant
        {
            if (auto const* const let = std::get_if<cst::expression::Conditional_let>(
                    &context.cst.expressions[loop.condition].variant)) {
                return desugar_while_let_loop(loop, *let);
            }
            return desugar_while_loop(loop);
        }

        auto operator()(cst::expression::Loop const& loop) const -> ast::Expression_variant
        {
            return ast::expression::Loop {
                .body   = desugar(context, loop.body),
                .source = ast::Loop_source::plain_loop,
            };
        }

        auto operator()(cst::expression::Function_call const& call) const -> ast::Expression_variant
        {
            return ast::expression::Function_call {
                .arguments = desugar(context, call.arguments.value.elements),
                .invocable = desugar(context, call.invocable),
            };
        }

        auto operator()(cst::expression::Tuple_initializer const& initializer) const
            -> ast::Expression_variant
        {
            return ast::expression::Tuple_initializer {
                .constructor_path = desugar(context, initializer.constructor_path),
                .initializers     = desugar(context, initializer.initializers),
            };
        }

        auto operator()(cst::expression::Struct_initializer const& initializer) const
            -> ast::Expression_variant
        {
            if (check_has_duplicate_fields(context, initializer)) {
                return ast::Error {};
            }
            return ast::expression::Struct_initializer {
                .constructor_path = desugar(context, initializer.constructor_path),
                .initializers     = desugar(context, initializer.initializers),
            };
        }

        auto operator()(cst::expression::Infix_call const& call) const -> ast::Expression_variant
        {
            return ast::expression::Infix_call {
                .left     = desugar(context, call.left),
                .right    = desugar(context, call.right),
                .op       = call.op,
                .op_range = context.cst.tokens[call.op_token].range,
            };
        }

        auto operator()(cst::expression::Struct_field const& field) const -> ast::Expression_variant
        {
            return ast::expression::Struct_field {
                .base_expression = desugar(context, field.base_expression),
                .field_name      = field.name,
            };
        }

        auto operator()(cst::expression::Tuple_field const& field) const -> ast::Expression_variant
        {
            return ast::expression::Tuple_field {
                .base_expression   = desugar(context, field.base_expression),
                .field_index       = field.field_index,
                .field_index_range = context.cst.tokens[field.field_index_token].range,
            };
        }

        auto operator()(cst::expression::Array_index const& field) const -> ast::Expression_variant
        {
            return ast::expression::Array_index {
                .base_expression  = desugar(context, field.base_expression),
                .index_expression = desugar(context, field.index_expression),
            };
        }

        auto operator()(cst::expression::Method_call const& call) const -> ast::Expression_variant
        {
            return ast::expression::Method_call {
                .function_arguments = desugar(context, call.function_arguments.value.elements),
                .template_arguments = call.template_arguments.transform(desugar(context)),
                .base_expression    = desugar(context, call.base_expression),
                .method_name        = call.method_name,
            };
        }

        auto operator()(cst::expression::Ascription const& ascription) const
            -> ast::Expression_variant
        {
            return ast::expression::Type_ascription {
                .expression    = desugar(context, ascription.base_expression),
                .ascribed_type = desugar(context, ascription.ascribed_type),
            };
        }

        auto operator()(cst::expression::Let const& let) const -> ast::Expression_variant
        {
            return ast::expression::Let {
                .pattern     = desugar(context, let.pattern),
                .initializer = desugar(context, let.initializer),
                .type        = let.type.has_value()
                                 ? desugar(context, let.type.value())
                                 : context.ast.types.push(wildcard_type(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Type_alias const& alias) const -> ast::Expression_variant
        {
            return ast::expression::Type_alias {
                .name = alias.name,
                .type = desugar(context, alias.type),
            };
        }

        auto operator()(cst::expression::Ret const& ret) const -> ast::Expression_variant
        {
            return ast::expression::Ret {
                ret.returned_expression.has_value()
                    ? desugar(context, ret.returned_expression.value())
                    : context.ast.expressions.push(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Break const& break_expression) const
            -> ast::Expression_variant
        {
            return ast::expression::Break {
                break_expression.result.has_value()
                    ? desugar(context, break_expression.result.value())
                    : context.ast.expressions.push(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Continue const&) const -> ast::Expression_variant
        {
            return ast::expression::Continue {};
        }

        auto operator()(cst::expression::Sizeof const& sizeof_) const -> ast::Expression_variant
        {
            return ast::expression::Sizeof { desugar(context, sizeof_.inspected_type.value) };
        }

        auto operator()(cst::expression::Addressof const& addressof) const
            -> ast::Expression_variant
        {
            auto const ampersand_range = context.cst.tokens[addressof.ampersand_token].range;
            return ast::expression::Addressof {
                .mutability       = desugar_mutability(addressof.mutability, ampersand_range),
                .place_expression = desugar(context, addressof.place_expression),
            };
        }

        auto operator()(cst::expression::Dereference const& dereference) const
            -> ast::Expression_variant
        {
            return ast::expression::Dereference {
                .reference_expression = desugar(context, dereference.reference_expression),
            };
        }

        auto operator()(cst::expression::Move const& move) const -> ast::Expression_variant
        {
            return ast::expression::Move { desugar(context, move.place_expression) };
        }

        auto operator()(cst::expression::Defer const& defer) const -> ast::Expression_variant
        {
            return ast::expression::Defer { desugar(context, defer.effect_expression) };
        }

        auto operator()(cst::expression::For_loop const&) const -> ast::Expression_variant
        {
            kieli::add_error(
                context.db,
                context.document_id,
                this_expression.range,
                "For loops are not supported yet");
            return ast::Error {};
        }

        auto operator()(cst::expression::Conditional_let const&) const -> ast::Expression_variant
        {
            // Should be unreachable because a conditional let expression can
            // only occur as the condition of if-let or while-let expressions.
            cpputil::unreachable();
        }
    };
} // namespace

auto libdesugar::desugar(Context const context, cst::Expression const& expression)
    -> ast::Expression
{
    Expression_desugaring_visitor visitor { context, expression };
    return { std::visit(visitor, expression.variant), expression.range };
}
