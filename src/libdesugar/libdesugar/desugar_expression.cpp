#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>

using namespace ki::desugar;

namespace {
    auto constant_loop_condition_diagnostic(ki::Range range, bool condition) -> ki::Diagnostic
    {
        return ki::Diagnostic {
            .message  = condition ? "Use `loop` instead of `while true`" : "Loop will never run",
            .range    = range,
            .severity = condition ? ki::Severity::Information : ki::Severity::Warning,
            .related_info = {},
        };
    }

    auto duplicate_fields_error(
        Context ctx, std::string_view name, ki::Range first, ki::Range second) -> ki::Diagnostic
    {
        return ki::Diagnostic {
            .message      = std::format("Duplicate initializer for struct member {}", name),
            .range        = second,
            .severity     = ki::Severity::Error,
            .related_info = utl::to_vector({
                ki::Diagnostic_related {
                    .message = "First specified here",
                    .location { .doc_id = ctx.doc_id, .range = first },
                },
            }),
        };
    }

    auto check_has_duplicate_fields(
        Context const ctx, cst::expression::Struct_init const& structure) -> bool
    {
        auto const& fields = structure.fields.value.elements;
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            auto const duplicate = std::ranges::find(
                it + 1, fields.end(), it->name.id, [](auto const& field) { return field.name.id; });
            if (duplicate != fields.end()) {
                ki::Diagnostic diagnostic = duplicate_fields_error(
                    ctx,
                    ctx.db.string_pool.get(it->name.id),
                    it->name.range,
                    duplicate->name.range);
                ki::add_diagnostic(ctx.db, ctx.doc_id, std::move(diagnostic));
                return true;
            }
        }
        return false;
    }

    auto break_expression(Context const ctx, ki::Range const range) -> ast::Expression_id
    {
        return ctx.ast.expr.push(
            ast::expression::Break { ctx.ast.expr.push(unit_value(range)) }, range);
    }

    struct Expression_desugaring_visitor {
        Context                ctx;
        cst::Expression const& this_expression;

        auto operator()(
            utl::one_of<ki::Integer, ki::Floating, ki::Boolean, ki::String, ki::Error> auto const&
                passthrough) const -> ast::Expression_variant
        {
            return passthrough;
        }

        auto operator()(cst::Path const& path) const -> ast::Expression_variant
        {
            return desugar(ctx, path);
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Expression_variant
        {
            return ast::Wildcard { .range = ctx.cst.range[wildcard.underscore_token] };
        }

        auto operator()(cst::expression::Paren const& paren) const -> ast::Expression_variant
        {
            cst::Expression const&        expr = ctx.cst.expr[paren.expression.value];
            Expression_desugaring_visitor visitor { .ctx = ctx, .this_expression = expr };
            return std::visit(visitor, expr.variant);
        }

        auto operator()(cst::expression::Array const& literal) const -> ast::Expression_variant
        {
            return ast::expression::Array { std::ranges::to<std::vector>(
                std::views::transform(literal.elements.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::expression::Tuple const& tuple) const -> ast::Expression_variant
        {
            return ast::expression::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.fields.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::expression::Conditional const& conditional) const
            -> ast::Expression_variant
        {
            ast::Expression_id const false_branch
                = conditional.false_branch.has_value()
                    ? desugar(ctx, conditional.false_branch->body)
                    : ctx.ast.expr.push(unit_value(this_expression.range));

            if (auto const* const let
                = std::get_if<cst::expression::Let>(&ctx.cst.expr[conditional.condition].variant)) {
                /*
                    if let a = b { c } else { d }

                    is transformed into

                    match b {
                        a -> c
                        _ -> d
                    }
                */

                auto initializer = desugar(ctx, let->initializer);

                if (let->type.has_value()) {
                    initializer = ctx.ast.expr.push(ast::Expression {
                        .variant = ast::expression::Type_ascription {
                            .expression    = initializer,
                            .type = desugar(ctx, let->type.value()),
                        },
                        .range = ctx.cst.expr[let->initializer].range,
                    });
                }

                return ast::expression::Match {
                    .cases = utl::to_vector({
                        ast::Match_case {
                            .pattern    = desugar(ctx, let->pattern),
                            .expression = desugar(ctx, conditional.true_branch),
                        },
                        ast::Match_case {
                            .pattern
                            = ctx.ast.patt.push(wildcard_pattern(ctx.cst.patt[let->pattern].range)),
                            .expression = false_branch,
                        },
                    }),

                    .scrutinee = initializer,
                };
            }

            ast::Expression condition = deref_desugar(ctx, conditional.condition);

            if (std::holds_alternative<ki::Boolean>(condition.variant)) {
                ki::Diagnostic diagnostic {
                    .message      = "Constant condition",
                    .range        = condition.range,
                    .severity     = ki::Severity::Information,
                    .related_info = {},
                };
                ki::add_diagnostic(ctx.db, ctx.doc_id, std::move(diagnostic));
            }

            return ast::expression::Conditional {
                .condition    = ctx.ast.expr.push(std::move(condition)),
                .true_branch  = desugar(ctx, conditional.true_branch),
                .false_branch = false_branch,
                .source
                = conditional.is_elif ? ast::Conditional_source::Elif : ast::Conditional_source::If,
                .has_explicit_false_branch = conditional.false_branch.has_value(),
            };
        }

        auto operator()(cst::expression::Match const& match) const -> ast::Expression_variant
        {
            auto const desugar_case = [&](cst::Match_case const& match_case) {
                return ast::Match_case {
                    .pattern    = desugar(ctx, match_case.pattern),
                    .expression = desugar(ctx, match_case.handler),
                };
            };
            return ast::expression::Match {
                .cases = std::views::transform(match.cases.value, desugar_case)
                       | std::ranges::to<std::vector>(),
                .scrutinee = desugar(ctx, match.scrutinee),
            };
        }

        auto operator()(cst::expression::Block const& block) const -> ast::Expression_variant
        {
            auto desugar_effect
                = [&](auto const& effect) { return deref_desugar(ctx, effect.expression); };

            auto side_effects = std::views::transform(block.side_effects, desugar_effect)
                              | std::ranges::to<std::vector>();

            cst::Range_id const unit_token = //
                block.side_effects.empty()   //
                    ? block.close_brace_token
                    : block.side_effects.back().trailing_semicolon_token;

            ast::Expression_id const result = //
                block.result_expression.has_value()
                    ? desugar(ctx, block.result_expression.value())
                    : ctx.ast.expr.push(unit_value(ctx.cst.range[unit_token]));

            return ast::expression::Block {
                .side_effects = std::move(side_effects),
                .result       = result,
            };
        }

        [[nodiscard]] auto desugar_while_let_loop(
            cst::expression::While_loop const& loop, cst::expression::Let const& let) const
            -> ast::Expression_variant
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

            auto initializer = desugar(ctx, let.initializer);

            if (let.type.has_value()) {
                initializer = ctx.ast.expr.push(ast::Expression {
                    .variant = ast::expression::Type_ascription {
                        .expression    = initializer,
                        .type = desugar(ctx, let.type.value()),
                    },
                    .range = ctx.cst.expr[let.initializer].range,
                });
            }

            auto cases = utl::to_vector({
                ast::Match_case {
                    .pattern    = desugar(ctx, let.pattern),
                    .expression = desugar(ctx, loop.body),
                },
                ast::Match_case {
                    .pattern    = ctx.ast.patt.push(wildcard_pattern(this_expression.range)),
                    .expression = break_expression(ctx, this_expression.range),
                },
            });

            return ast::expression::Loop {
                .body = ctx.ast.expr.push(
                    ast::expression::Match {
                        .cases     = std::move(cases),
                        .scrutinee = initializer,
                    },
                    ctx.cst.expr[loop.body].range),
                .source = ast::Loop_source::While_loop,
            };
        }

        [[nodiscard]] auto desugar_while_loop(cst::expression::While_loop const& loop) const
            -> ast::Expression_variant
        {
            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */
            ast::Expression condition = deref_desugar(ctx, loop.condition);
            if (auto const* const boolean = std::get_if<ki::Boolean>(&condition.variant)) {
                ki::add_diagnostic(
                    ctx.db,
                    ctx.doc_id,
                    constant_loop_condition_diagnostic(condition.range, boolean->value));
            }
            return ast::expression::Loop {
                .body = ctx.ast.expr.push(
                    ast::expression::Conditional {
                        .condition                 = ctx.ast.expr.push(std::move(condition)),
                        .true_branch               = desugar(ctx, loop.body),
                        .false_branch              = break_expression(ctx, this_expression.range),
                        .source                    = ast::Conditional_source::While,
                        .has_explicit_false_branch = true,
                    },
                    ctx.cst.expr[loop.body].range),
                .source = ast::Loop_source::While_loop,
            };
        }

        auto operator()(cst::expression::While_loop const& loop) const -> ast::Expression_variant
        {
            if (auto const* const let
                = std::get_if<cst::expression::Let>(&ctx.cst.expr[loop.condition].variant)) {
                return desugar_while_let_loop(loop, *let);
            }
            return desugar_while_loop(loop);
        }

        auto operator()(cst::expression::Loop const& loop) const -> ast::Expression_variant
        {
            return ast::expression::Loop {
                .body   = desugar(ctx, loop.body),
                .source = ast::Loop_source::Plain_loop,
            };
        }

        auto operator()(cst::expression::Function_call const& call) const -> ast::Expression_variant
        {
            return ast::expression::Function_call {
                .arguments = desugar(ctx, call.arguments.value.elements),
                .invocable = desugar(ctx, call.invocable),
            };
        }

        auto operator()(cst::expression::Tuple_init const& init) const -> ast::Expression_variant
        {
            return ast::expression::Tuple_initializer {
                .constructor_path = desugar(ctx, init.path),
                .initializers     = desugar(ctx, init.fields),
            };
        }

        auto operator()(cst::expression::Struct_init const& init) const -> ast::Expression_variant
        {
            if (check_has_duplicate_fields(ctx, init)) {
                return ki::Error {};
            }
            return ast::expression::Struct_initializer {
                .constructor_path = desugar(ctx, init.path),
                .initializers     = desugar(ctx, init.fields),
            };
        }

        auto operator()(cst::expression::Infix_call const& call) const -> ast::Expression_variant
        {
            return ast::expression::Infix_call {
                .left     = desugar(ctx, call.left),
                .right    = desugar(ctx, call.right),
                .op       = call.op,
                .op_range = ctx.cst.range[call.op_token],
            };
        }

        auto operator()(cst::expression::Struct_field const& field) const -> ast::Expression_variant
        {
            return ast::expression::Struct_field {
                .base_expression = desugar(ctx, field.base_expression),
                .field_name      = field.name,
            };
        }

        auto operator()(cst::expression::Tuple_field const& field) const -> ast::Expression_variant
        {
            return ast::expression::Tuple_field {
                .base_expression   = desugar(ctx, field.base_expression),
                .field_index       = field.field_index,
                .field_index_range = ctx.cst.range[field.field_index_token],
            };
        }

        auto operator()(cst::expression::Array_index const& field) const -> ast::Expression_variant
        {
            return ast::expression::Array_index {
                .base_expression  = desugar(ctx, field.base_expression),
                .index_expression = desugar(ctx, field.index_expression),
            };
        }

        auto operator()(cst::expression::Method_call const& call) const -> ast::Expression_variant
        {
            return ast::expression::Method_call {
                .function_arguments = desugar(ctx, call.function_arguments.value.elements),
                .template_arguments = call.template_arguments.transform(desugar(ctx)),
                .base_expression    = desugar(ctx, call.base_expression),
                .method_name        = call.method_name,
            };
        }

        auto operator()(cst::expression::Ascription const& ascription) const
            -> ast::Expression_variant
        {
            return ast::expression::Type_ascription {
                .expression = desugar(ctx, ascription.base_expression),
                .type       = desugar(ctx, ascription.type),
            };
        }

        auto operator()(cst::expression::Let const& let) const -> ast::Expression_variant
        {
            return ast::expression::Let {
                .pattern     = desugar(ctx, let.pattern),
                .initializer = desugar(ctx, let.initializer),
                .type        = let.type.has_value()
                                 ? desugar(ctx, let.type.value())
                                 : ctx.ast.type.push(wildcard_type(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Type_alias const& alias) const -> ast::Expression_variant
        {
            return ast::expression::Type_alias {
                .name = alias.name,
                .type = desugar(ctx, alias.type),
            };
        }

        auto operator()(cst::expression::Ret const& ret) const -> ast::Expression_variant
        {
            return ast::expression::Ret {
                ret.returned_expression.has_value()
                    ? desugar(ctx, ret.returned_expression.value())
                    : ctx.ast.expr.push(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Break const& break_expression) const
            -> ast::Expression_variant
        {
            return ast::expression::Break {
                break_expression.result.has_value()
                    ? desugar(ctx, break_expression.result.value())
                    : ctx.ast.expr.push(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Continue const&) const -> ast::Expression_variant
        {
            return ast::expression::Continue {};
        }

        auto operator()(cst::expression::Sizeof const& sizeof_) const -> ast::Expression_variant
        {
            return ast::expression::Sizeof { desugar(ctx, sizeof_.type.value) };
        }

        auto operator()(cst::expression::Addressof const& addressof) const
            -> ast::Expression_variant
        {
            return ast::expression::Addressof {
                .mutability = desugar_mutability(
                    addressof.mutability, ctx.cst.range[addressof.ampersand_token]),
                .place_expression = desugar(ctx, addressof.place_expression),
            };
        }

        auto operator()(cst::expression::Dereference const& dereference) const
            -> ast::Expression_variant
        {
            return ast::expression::Dereference {
                .reference_expression = desugar(ctx, dereference.reference_expression),
            };
        }

        auto operator()(cst::expression::Defer const& defer) const -> ast::Expression_variant
        {
            return ast::expression::Defer { desugar(ctx, defer.effect_expression) };
        }

        auto operator()(cst::expression::For_loop const&) const -> ast::Expression_variant
        {
            ki::add_error(
                ctx.db, ctx.doc_id, this_expression.range, "For loops are not supported yet");
            return ki::Error {};
        }
    };
} // namespace

auto ki::desugar::desugar(Context const ctx, cst::Expression const& expression) -> ast::Expression
{
    Expression_desugaring_visitor visitor { .ctx = ctx, .this_expression = expression };
    return { .variant = std::visit(visitor, expression.variant), .range = expression.range };
}
