#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>

using namespace ki;
using namespace ki::des;

namespace {
    auto constant_loop_condition_diagnostic(lsp::Range range, bool constant) -> lsp::Diagnostic
    {
        return lsp::Diagnostic {
            .message      = constant ? "Use 'loop' instead of 'while true'" : "Loop will never run",
            .range        = range,
            .severity     = constant ? lsp::Severity::Information : lsp::Severity::Warning,
            .related_info = {},
            .tag          = lsp::Diagnostic_tag::None,
        };
    }

    auto break_expression(Context& ctx, lsp::Range const range) -> ast::Expression_id
    {
        return ctx.ast.expressions.push(
            ast::expr::Break { ctx.ast.expressions.push(unit_value(range)) }, range);
    }

    struct Visitor {
        Context&      ctx;
        cst::Range_id range_id;

        auto operator()(utl::one_of< //
                        db::Integer,
                        db::Floating,
                        db::Boolean,
                        db::String,
                        db::Error> auto const& passthrough) const -> ast::Expression_variant
        {
            return passthrough;
        }

        auto operator()(cst::Path const& path) const -> ast::Expression_variant
        {
            return desugar(ctx, path);
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Expression_variant
        {
            return ast::Wildcard { .range = ctx.cst.ranges[wildcard.underscore_token] };
        }

        auto operator()(cst::expr::Paren const& paren) const -> ast::Expression_variant
        {
            cst::Expression const& expr = ctx.cst.expressions[paren.expression.value];
            return std::visit(Visitor { .ctx = ctx, .range_id = expr.range }, expr.variant);
        }

        auto operator()(cst::expr::Array const& literal) const -> ast::Expression_variant
        {
            return ast::expr::Array { .elements = desugar(ctx, literal.elements) };
        }

        auto operator()(cst::expr::Tuple const& tuple) const -> ast::Expression_variant
        {
            return ast::expr::Tuple { .fields = desugar(ctx, tuple.fields) };
        }

        auto operator()(cst::expr::Conditional const& conditional) const -> ast::Expression_variant
        {
            ast::Expression_id const false_branch
                = conditional.false_branch.has_value()
                    ? desugar(ctx, conditional.false_branch->body)
                    : ctx.ast.expressions.push(unit_value(ctx.cst.ranges[range_id]));

            if (auto const* const let = std::get_if<cst::expr::Let>(
                    &ctx.cst.expressions[conditional.condition].variant)) {
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
                    initializer = ctx.ast.expressions.push(ast::Expression {
                        .variant = ast::expr::Ascription {
                            .expression = initializer,
                            .type       = desugar(ctx, let->type.value()),
                        },
                        .range = ctx.cst.ranges[ctx.cst.expressions[let->initializer].range],
                    });
                }

                return ast::expr::Match {
                    .arms = utl::to_vector(
                        {
                            ast::Match_arm {
                                .pattern    = desugar(ctx, let->pattern),
                                .expression = desugar(ctx, conditional.true_branch),
                            },
                            ast::Match_arm {
                                .pattern    = ctx.ast.patterns.push(wildcard_pattern(
                                    ctx.cst.ranges[ctx.cst.patterns[let->pattern].range])),
                                .expression = false_branch,
                            },
                        }),

                    .scrutinee = initializer,
                };
            }

            ast::Expression condition = deref_desugar(ctx, conditional.condition);

            if (std::holds_alternative<db::Boolean>(condition.variant)) {
                lsp::Diagnostic diagnostic {
                    .message      = "Constant condition",
                    .range        = condition.range,
                    .severity     = lsp::Severity::Information,
                    .related_info = {},
                    .tag          = lsp::Diagnostic_tag::None,
                };
                db::add_diagnostic(ctx.db, ctx.doc_id, std::move(diagnostic));
            }

            return ast::expr::Conditional {
                .condition                 = ctx.ast.expressions.push(std::move(condition)),
                .true_branch               = desugar(ctx, conditional.true_branch),
                .false_branch              = false_branch,
                .source                    = ast::Conditional_source::If,
                .has_explicit_false_branch = conditional.false_branch.has_value(),
            };
        }

        auto operator()(cst::expr::Match const& match) const -> ast::Expression_variant
        {
            auto const desugar_arm = [&](cst::Match_arm const& arm) {
                return ast::Match_arm {
                    .pattern    = desugar(ctx, arm.pattern),
                    .expression = desugar(ctx, arm.handler),
                };
            };
            return ast::expr::Match {
                .arms = std::views::transform(match.arms.value, desugar_arm)
                      | std::ranges::to<std::vector>(),
                .scrutinee = desugar(ctx, match.scrutinee),
            };
        }

        auto operator()(cst::expr::Block const& block) const -> ast::Expression_variant
        {
            auto side_effects
                = std::views::transform(
                      block.effects,
                      [&](auto const& effect) { return desugar(ctx, effect.expression); })
                | std::ranges::to<std::vector>();

            cst::Range_id unit_token = //
                block.effects.empty()  //
                    ? block.close_brace_token
                    : block.effects.back().trailing_semicolon_token;

            ast::Expression_id result = //
                block.result.has_value()
                    ? desugar(ctx, block.result.value())
                    : ctx.ast.expressions.push(unit_value(ctx.cst.ranges[unit_token]));

            return ast::expr::Block {
                .effects = std::move(side_effects),
                .result  = result,
            };
        }

        [[nodiscard]] auto desugar_while_let_loop(
            cst::expr::While_loop const& loop, cst::expr::Let const& let) const
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
                initializer = ctx.ast.expressions.push(ast::Expression {
                    .variant = ast::expr::Ascription {
                        .expression = initializer,
                        .type       = desugar(ctx, let.type.value()),
                    },
                    .range = ctx.cst.ranges[ctx.cst.expressions[let.initializer].range],
                });
            }

            auto arms = utl::to_vector(
                {
                    ast::Match_arm {
                        .pattern    = desugar(ctx, let.pattern),
                        .expression = desugar(ctx, loop.body),
                    },
                    ast::Match_arm {
                        .pattern
                        = ctx.ast.patterns.push(wildcard_pattern(ctx.cst.ranges[range_id])),
                        .expression = break_expression(ctx, ctx.cst.ranges[range_id]),
                    },
                });

            return ast::expr::Loop {
                .body = ctx.ast.expressions.push(
                    ast::expr::Match {
                        .arms      = std::move(arms),
                        .scrutinee = initializer,
                    },
                    ctx.cst.ranges[ctx.cst.expressions[loop.body].range]),
                .source = ast::Loop_source::While_loop,
            };
        }

        [[nodiscard]] auto desugar_while_loop(cst::expr::While_loop const& loop) const
            -> ast::Expression_variant
        {
            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */
            ast::Expression condition = deref_desugar(ctx, loop.condition);
            if (auto const* const boolean = std::get_if<db::Boolean>(&condition.variant)) {
                auto diag = constant_loop_condition_diagnostic(condition.range, boolean->value);
                db::add_diagnostic(ctx.db, ctx.doc_id, std::move(diag));
            }
            return ast::expr::Loop {
                .body = ctx.ast.expressions.push(
                    ast::expr::Conditional {
                        .condition    = ctx.ast.expressions.push(std::move(condition)),
                        .true_branch  = desugar(ctx, loop.body),
                        .false_branch = break_expression(ctx, ctx.cst.ranges[range_id]),
                        .source       = ast::Conditional_source::While,
                        .has_explicit_false_branch = true,
                    },
                    ctx.cst.ranges[ctx.cst.expressions[loop.body].range]),
                .source = ast::Loop_source::While_loop,
            };
        }

        auto operator()(cst::expr::While_loop const& loop) const -> ast::Expression_variant
        {
            if (auto const* const let
                = std::get_if<cst::expr::Let>(&ctx.cst.expressions[loop.condition].variant)) {
                return desugar_while_let_loop(loop, *let);
            }
            return desugar_while_loop(loop);
        }

        auto operator()(cst::expr::Loop const& loop) const -> ast::Expression_variant
        {
            return ast::expr::Loop {
                .body   = desugar(ctx, loop.body),
                .source = ast::Loop_source::Plain_loop,
            };
        }

        auto operator()(cst::expr::Function_call const& call) const -> ast::Expression_variant
        {
            return ast::expr::Function_call {
                .arguments = desugar(ctx, call.arguments.value.elements),
                .invocable = desugar(ctx, call.invocable),
            };
        }

        auto operator()(cst::expr::Struct_init const& init) const -> ast::Expression_variant
        {
            return ast::expr::Struct_init {
                .path   = desugar(ctx, init.path),
                .fields = desugar(ctx, init.fields),
            };
        }

        auto operator()(cst::expr::Infix_call const& call) const -> ast::Expression_variant
        {
            return ast::expr::Infix_call {
                .left  = desugar(ctx, call.left),
                .right = desugar(ctx, call.right),
                .op    = call.op,
            };
        }

        auto operator()(cst::expr::Struct_field const& field) const -> ast::Expression_variant
        {
            return ast::expr::Struct_field {
                .base = desugar(ctx, field.base),
                .name = field.name,
            };
        }

        auto operator()(cst::expr::Tuple_field const& field) const -> ast::Expression_variant
        {
            return ast::expr::Tuple_field {
                .base        = desugar(ctx, field.base),
                .index       = field.index,
                .index_range = ctx.cst.ranges[field.index_token],
            };
        }

        auto operator()(cst::expr::Array_index const& field) const -> ast::Expression_variant
        {
            return ast::expr::Array_index {
                .base  = desugar(ctx, field.base),
                .index = desugar(ctx, field.index),
            };
        }

        auto operator()(cst::expr::Method_call const& call) const -> ast::Expression_variant
        {
            return ast::expr::Method_call {
                .function_arguments = desugar(ctx, call.function_arguments.value.elements),
                .template_arguments = call.template_arguments.transform(desugar(ctx)),
                .expression         = desugar(ctx, call.expression),
                .name               = call.name,
            };
        }

        auto operator()(cst::expr::Ascription const& ascription) const -> ast::Expression_variant
        {
            return ast::expr::Ascription {
                .expression = desugar(ctx, ascription.expression),
                .type       = desugar(ctx, ascription.type),
            };
        }

        auto operator()(cst::expr::Let const& let) const -> ast::Expression_variant
        {
            return ast::expr::Let {
                .pattern     = desugar(ctx, let.pattern),
                .initializer = desugar(ctx, let.initializer),
                .type        = let.type.transform(desugar(ctx)),
            };
        }

        auto operator()(cst::expr::Type_alias const& alias) const -> ast::Expression_variant
        {
            return ast::expr::Type_alias {
                .name = alias.name,
                .type = desugar(ctx, alias.type),
            };
        }

        auto operator()(cst::expr::Return const& ret) const -> ast::Expression_variant
        {
            return ast::expr::Return {
                ret.expression.has_value()
                    ? desugar(ctx, ret.expression.value())
                    : ctx.ast.expressions.push(unit_value(ctx.cst.ranges[range_id])),
            };
        }

        auto operator()(cst::expr::Break const& break_) const -> ast::Expression_variant
        {
            return ast::expr::Break {
                break_.result.has_value()
                    ? desugar(ctx, break_.result.value())
                    : ctx.ast.expressions.push(unit_value(ctx.cst.ranges[range_id])),
            };
        }

        auto operator()(cst::expr::Continue const&) const -> ast::Expression_variant
        {
            return ast::expr::Continue {};
        }

        auto operator()(cst::expr::Sizeof const& sizeof_) const -> ast::Expression_variant
        {
            return ast::expr::Sizeof { desugar(ctx, sizeof_.type.value) };
        }

        auto operator()(cst::expr::Addressof const& addressof) const -> ast::Expression_variant
        {
            return ast::expr::Addressof {
                .mutability = desugar_opt_mut(
                    ctx, addressof.mutability, ctx.cst.ranges[addressof.ampersand_token]),
                .expression = desugar(ctx, addressof.expression),
            };
        }

        auto operator()(cst::expr::Deref const& dereference) const -> ast::Expression_variant
        {
            return ast::expr::Deref { desugar(ctx, dereference.expression) };
        }

        auto operator()(cst::expr::Defer const& defer) const -> ast::Expression_variant
        {
            return ast::expr::Defer { desugar(ctx, defer.expression) };
        }

        auto operator()(cst::expr::For_loop const& loop) const -> ast::Expression_variant
        {
            db::add_error(
                ctx.db,
                ctx.doc_id,
                ctx.cst.ranges[loop.for_token],
                "For loops are not supported yet");
            return db::Error {};
        }
    };
} // namespace

auto ki::des::desugar(Context& ctx, cst::Expression const& expr) -> ast::Expression
{
    return ast::Expression {
        .variant = std::visit(Visitor { .ctx = ctx, .range_id = expr.range }, expr.variant),
        .range   = ctx.cst.ranges[expr.range],
    };
}
