#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

using namespace ki;
using namespace ki::fmt;

namespace {
    struct Expression_format_visitor {
        Context& ctx;

        void format_indented_block_body(cst::expr::Block const& block)
        {
            std::print(ctx.stream, "{{");
            indent(ctx, [&] {
                for (auto const& side_effect : block.effects) {
                    newline(ctx);
                    format(ctx, side_effect.expression);
                    std::print(ctx.stream, ";");
                }
                if (block.result.has_value()) {
                    newline(ctx);
                    format(ctx, block.result.value());
                }
            });
            newline(ctx);
            std::print(ctx.stream, "}}");
        }

        void format_regular_block(cst::expr::Block const& block)
        {
            if (block.effects.empty()) {
                if (block.result.has_value()) {
                    std::print(ctx.stream, "{{ ");
                    format(ctx, block.result.value());
                    std::print(ctx.stream, " }}");
                }
                else {
                    std::print(ctx.stream, "{{}}");
                }
            }
            else {
                format_indented_block_body(block);
            }
        }

        auto as_block(cst::Expression_id const id) -> cst::expr::Block const&
        {
            auto const& expression = ctx.arena.expressions[id];
            auto const* block      = std::get_if<cst::expr::Block>(&expression.variant);
            cpputil::always_assert(block != nullptr);
            return *block;
        }

        void operator()(utl::one_of<db::Integer, db::Floating, db::Boolean> auto const literal)
        {
            std::print(ctx.stream, "{}", literal.value);
        }

        void operator()(db::String const string)
        {
            std::print(ctx.stream, "{:?}", ctx.db.string_pool.get(string.id));
        }

        void operator()(cst::Wildcard const& wildcard)
        {
            format(ctx, wildcard);
        }

        void operator()(cst::Builtin const& builtin)
        {
            std::print(ctx.stream, "@{}", ctx.db.string_pool.get(builtin.name.id));
        }

        void operator()(cst::Path const& path)
        {
            format(ctx, path);
        }

        void operator()(cst::expr::Block const& block)
        {
            format_regular_block(block);
        }

        void operator()(cst::expr::Paren const& paren)
        {
            std::print(ctx.stream, "(");
            format(ctx, paren.expression.value);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::expr::Tuple const& tuple)
        {
            std::print(ctx.stream, "(");
            format_comma_separated(ctx, tuple.fields.value.elements);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::expr::Infix_call const& call)
        {
            format(ctx, call.left);
            std::print(ctx.stream, " {} ", ctx.db.string_pool.get(call.op.id));
            format(ctx, call.right);
        }

        void operator()(cst::expr::Function_call const& call)
        {
            format(ctx, call.invocable);
            format(ctx, call.arguments);
        }

        void operator()(cst::expr::Struct_init const& structure)
        {
            format(ctx, structure.path);
            std::print(ctx.stream, " {{ ");
            format_comma_separated(ctx, structure.fields.value.elements);
            std::print(ctx.stream, " }}");
        }

        void operator()(cst::expr::Method_call const& call)
        {
            format(ctx, call.expression);
            std::print(ctx.stream, ".{}", ctx.db.string_pool.get(call.name.id));
            format(ctx, call.template_arguments);
            format(ctx, call.function_arguments);
        }

        void operator()(cst::expr::Match const& match)
        {
            std::print(ctx.stream, "match ");
            format(ctx, match.scrutinee);
            std::print(ctx.stream, " {{");
            indent(ctx, [&] {
                for (auto const& arm : match.arms.value) {
                    newline(ctx);
                    format(ctx, arm.pattern);
                    std::print(ctx.stream, " -> ");
                    format(ctx, arm.handler);
                    if (arm.semicolon_token.has_value()) {
                        std::print(ctx.stream, ";");
                    }
                }
            });
            newline(ctx);
            std::print(ctx.stream, "}}");
        }

        void operator()(cst::expr::Sizeof const& sizeof_)
        {
            std::print(ctx.stream, "sizeof(");
            format(ctx, sizeof_.type.value);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::expr::Type_alias const& alias)
        {
            std::print(ctx.stream, "alias {} = ", ctx.db.string_pool.get(alias.name.id));
            format(ctx, alias.type);
        }

        void operator()(cst::expr::Let const& let)
        {
            std::print(ctx.stream, "let ");
            format(ctx, let.pattern);
            format(ctx, let.type);
            std::print(ctx.stream, " = ");
            format(ctx, let.initializer);
        }

        void operator()(cst::expr::Array const& array)
        {
            std::print(ctx.stream, "[");
            format_comma_separated(ctx, array.elements.value.elements);
            std::print(ctx.stream, "]");
        }

        void operator()(cst::expr::Tuple_field const& field)
        {
            format(ctx, field.base);
            std::print(ctx.stream, ".{}", field.index);
        }

        void operator()(cst::expr::Struct_field const& field)
        {
            format(ctx, field.base);
            std::print(ctx.stream, ".{}", ctx.db.string_pool.get(field.name.id));
        }

        void operator()(cst::expr::Array_index const& index)
        {
            format(ctx, index.base);
            std::print(ctx.stream, ".[");
            format(ctx, index.index.value);
            std::print(ctx.stream, "]");
        }

        void operator()(cst::expr::Addressof const& reference)
        {
            std::print(ctx.stream, "&");
            format_mutability_with_whitespace(ctx, reference.mutability);
            format(ctx, reference.expression);
        }

        void operator()(cst::expr::Deref const& dereference)
        {
            std::print(ctx.stream, "*");
            format(ctx, dereference.expression);
        }

        void operator()(cst::expr::Ascription const& ascription)
        {
            format(ctx, ascription.expression);
            std::print(ctx.stream, ": ");
            format(ctx, ascription.type);
        }

        void operator()(cst::expr::For_loop const& loop)
        {
            std::print(ctx.stream, "for ");
            format(ctx, loop.iterator);
            std::print(ctx.stream, " in ");
            format(ctx, loop.iterable);
            std::print(ctx.stream, " ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expr::While_loop const& loop)
        {
            std::print(ctx.stream, "while ");
            format(ctx, loop.condition);
            std::print(ctx.stream, " ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expr::Loop const& loop)
        {
            std::print(ctx.stream, "loop ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expr::Return const& ret)
        {
            if (ret.expression.has_value()) {
                std::print(ctx.stream, "ret ");
                format(ctx, ret.expression.value());
            }
            else {
                std::print(ctx.stream, "ret");
            }
        }

        void operator()(cst::expr::Conditional const& conditional)
        {
            std::print(ctx.stream, "if ");
            format(ctx, conditional.condition);
            std::print(ctx.stream, " ");
            format_indented_block_body(as_block(conditional.true_branch));
            if (not conditional.false_branch.has_value()) {
                return;
            }
            newline(ctx);
            std::print(ctx.stream, "else ");
            if (auto const* block = std::get_if<cst::expr::Block>(
                    &ctx.arena.expressions[conditional.false_branch.value().body].variant)) {
                format_indented_block_body(*block);
            }
            else {
                format(ctx, conditional.false_branch.value().body);
            }
        }

        void operator()(cst::expr::Continue const&)
        {
            std::print(ctx.stream, "continue");
        }

        void operator()(cst::expr::Break const& break_)
        {
            if (break_.result.has_value()) {
                std::print(ctx.stream, "break ");
                format(ctx, break_.result.value());
            }
            else {
                std::print(ctx.stream, "break");
            }
        }

        void operator()(cst::expr::Defer const& defer)
        {
            std::print(ctx.stream, "defer ");
            format(ctx, defer.expression);
        }

        void operator()(cst::expr::Pipe const& pipe)
        {
            format(ctx, pipe.left);
            std::print(ctx.stream, " | ");
            format(ctx, pipe.right);
        }

        void operator()(db::Error)
        {
            cpputil::todo();
        }
    };
} // namespace

void ki::fmt::format(Context& ctx, cst::Expression const& expression)
{
    std::visit(Expression_format_visitor { ctx }, expression.variant);
}
