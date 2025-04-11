#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

using namespace ki;
using namespace ki::fmt;

namespace {
    struct Expression_format_visitor {
        State& state;

        void format_indented_block_body(cst::expr::Block const& block)
        {
            format(state, "{{");
            indent(state, [&] {
                for (auto const& side_effect : block.side_effects) {
                    format(state, "{}", newline(state));
                    format(state, side_effect.expression);
                    format(state, ";");
                }
                if (block.result_expression.has_value()) {
                    format(state, "{}", newline(state));
                    format(state, block.result_expression.value());
                }
            });
            format(state, "{}}}", newline(state));
        }

        void format_regular_block(cst::expr::Block const& block)
        {
            if (block.side_effects.empty()) {
                if (block.result_expression.has_value()) {
                    format(state, "{{ ");
                    format(state, block.result_expression.value());
                    format(state, " }}");
                }
                else {
                    format(state, "{{}}");
                }
            }
            else {
                format_indented_block_body(block);
            }
        }

        auto as_block(cst::Expression_id const id) -> cst::expr::Block const&
        {
            auto const&       expression = state.arena.expressions[id];
            auto const* const block      = std::get_if<cst::expr::Block>(&expression.variant);
            cpputil::always_assert(block != nullptr);
            return *block;
        }

        void operator()(utl::one_of<db::Integer, db::Floating, db::Boolean> auto const literal)
        {
            format(state, "{}", literal.value);
        }

        void operator()(db::String const string)
        {
            format(state, "{:?}", state.pool.get(string.id));
        }

        void operator()(cst::Wildcard const& wildcard)
        {
            format(state, wildcard);
        }

        void operator()(cst::Path const& path)
        {
            format(state, path);
        }

        void operator()(cst::expr::Block const& block)
        {
            format_regular_block(block);
        }

        void operator()(cst::expr::Paren const& paren)
        {
            format(state, "(");
            format(state, paren.expression.value);
            format(state, ")");
        }

        void operator()(cst::expr::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.fields.value.elements);
            format(state, ")");
        }

        void operator()(cst::expr::Infix_call const& call)
        {
            format(state, call.left);
            format(state, " {} ", state.pool.get(call.op));
            format(state, call.right);
        }

        void operator()(cst::expr::Function_call const& call)
        {
            format(state, call.invocable);
            format(state, call.arguments);
        }

        void operator()(cst::expr::Tuple_init const& structure)
        {
            format(state, structure.path);
            format(state, "(");
            format_comma_separated(state, structure.fields.value.elements);
            format(state, ")");
        }

        void operator()(cst::expr::Struct_init const& structure)
        {
            format(state, structure.path);
            format(state, " {{ ");
            format_comma_separated(state, structure.fields.value.elements);
            format(state, " }}");
        }

        void operator()(cst::expr::Method_call const& call)
        {
            format(state, call.base_expression);
            format(state, ".{}", state.pool.get(call.method_name.id));
            format(state, call.template_arguments);
            format(state, call.function_arguments);
        }

        void operator()(cst::expr::Match const& match)
        {
            format(state, "match ");
            format(state, match.scrutinee);
            format(state, " {{");
            indent(state, [&] {
                for (auto const& arm : match.arms.value) {
                    format(state, "{}", newline(state));
                    format(state, arm.pattern);
                    format(state, " -> ");
                    format(state, arm.handler);
                    if (arm.semicolon_token.has_value()) {
                        format(state, ";");
                    }
                }
            });
            format(state, "{}}}", newline(state));
        }

        void operator()(cst::expr::Sizeof const& sizeof_)
        {
            format(state, "sizeof(");
            format(state, sizeof_.type.value);
            format(state, ")");
        }

        void operator()(cst::expr::Type_alias const& alias)
        {
            format(state, "alias {} = ", state.pool.get(alias.name.id));
            format(state, alias.type);
        }

        void operator()(cst::expr::Let const& let)
        {
            format(state, "let ");
            format(state, let.pattern);
            format(state, let.type);
            format(state, " = ");
            format(state, let.initializer);
        }

        void operator()(cst::expr::Array const& array)
        {
            format(state, "[");
            format_comma_separated(state, array.elements.value.elements);
            format(state, "]");
        }

        void operator()(cst::expr::Tuple_field const& field)
        {
            format(state, field.base_expression);
            format(state, ".{}", field.field_index);
        }

        void operator()(cst::expr::Struct_field const& field)
        {
            format(state, field.base_expression);
            format(state, ".{}", state.pool.get(field.name.id));
        }

        void operator()(cst::expr::Array_index const& index)
        {
            format(state, index.base_expression);
            format(state, ".[");
            format(state, index.index_expression.value);
            format(state, "]");
        }

        void operator()(cst::expr::Addressof const& reference)
        {
            format(state, "&");
            format_mutability_with_whitespace(state, reference.mutability);
            format(state, reference.expression);
        }

        void operator()(cst::expr::Deref const& dereference)
        {
            format(state, "*");
            format(state, dereference.expression);
        }

        void operator()(cst::expr::Ascription const& ascription)
        {
            format(state, ascription.base_expression);
            format(state, ": ");
            format(state, ascription.type);
        }

        void operator()(cst::expr::For_loop const& loop)
        {
            format(state, "for ");
            format(state, loop.iterator);
            format(state, " in ");
            format(state, loop.iterable);
            format(state, " ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expr::While_loop const& loop)
        {
            format(state, "while ");
            format(state, loop.condition);
            format(state, " ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expr::Loop const& loop)
        {
            format(state, "loop ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expr::Return const& ret)
        {
            if (ret.expression.has_value()) {
                format(state, "ret ");
                format(state, ret.expression.value());
            }
            else {
                format(state, "ret");
            }
        }

        void operator()(cst::expr::Conditional const& conditional)
        {
            format(state, "{} ", conditional.is_elif ? "elif" : "if");
            format(state, conditional.condition);
            format(state, " ");
            format_indented_block_body(as_block(conditional.true_branch));
            if (not conditional.false_branch.has_value()) {
                return;
            }
            if (auto const* const else_conditional = std::get_if<cst::expr::Conditional>(
                    &state.arena.expressions[conditional.false_branch.value().body].variant)) {
                if (else_conditional->is_elif) {
                    format(state, "{}", newline(state));
                    format(state, conditional.false_branch.value().body);
                    return;
                }
            }
            format(state, "{}else ", newline(state));
            format_indented_block_body(as_block(conditional.false_branch.value().body));
        }

        void operator()(cst::expr::Break const& break_)
        {
            if (break_.result.has_value()) {
                format(state, "break ");
                format(state, break_.result.value());
            }
            else {
                format(state, "break");
            }
        }

        void operator()(cst::expr::Defer const& defer)
        {
            format(state, "defer ");
            format(state, defer.expression);
        }

        void operator()(cst::expr::Continue const&)
        {
            format(state, "continue");
        }

        void operator()(db::Error)
        {
            cpputil::todo();
        }
    };
} // namespace

void ki::fmt::format(State& state, cst::Expression const& expression)
{
    std::visit(Expression_format_visitor { state }, expression.variant);
}
