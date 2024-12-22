#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

using namespace libformat;

namespace {
    struct Expression_format_visitor {
        State& state;

        void format_indented_block_body(cst::expression::Block const& block)
        {
            format(state, "{{");
            {
                auto const _ = state.indent();
                for (auto const& side_effect : block.side_effects) {
                    format(state, "{}", state.newline());
                    format(state, side_effect.expression);
                    format(state, ";");
                }
                if (block.result_expression.has_value()) {
                    format(state, "{}", state.newline());
                    format(state, block.result_expression.value());
                }
            }
            format(state, "{}}}", state.newline());
        }

        void format_regular_block(cst::expression::Block const& block)
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

        auto as_block(cst::Expression_id const id) -> cst::expression::Block const&
        {
            auto const&       expression = state.arena.expressions[id];
            auto const* const block      = std::get_if<cst::expression::Block>(&expression.variant);
            cpputil::always_assert(block != nullptr);
            return *block;
        }

        template <kieli::literal Literal>
        void operator()(Literal const& literal)
        {
            if constexpr (utl::one_of<Literal, kieli::Character, kieli::String>) {
                format(state, "{:?}", literal.value);
            }
            else {
                format(state, "{}", literal.value);
            }
        }

        void operator()(cst::Wildcard const& wildcard)
        {
            format(state, wildcard);
        }

        void operator()(cst::Path const& path)
        {
            format(state, path);
        }

        void operator()(cst::expression::Block const& block)
        {
            format_regular_block(block);
        }

        void operator()(cst::expression::Paren const& paren)
        {
            format(state, "(");
            format(state, paren.expression.value);
            format(state, ")");
        }

        void operator()(cst::expression::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.fields.value.elements);
            format(state, ")");
        }

        void operator()(cst::expression::Infix_chain const& sequence)
        {
            format(state, sequence.lhs);
            for (auto const& [rhs, op] : sequence.tail) {
                format(state, " {} ", op.identifier);
                format(state, rhs);
            }
        }

        void operator()(cst::expression::Conditional_let const& let)
        {
            format(state, "let ");
            format(state, let.pattern);
            format(state, " = ");
            format(state, let.initializer);
        }

        void operator()(cst::expression::Function_call const& call)
        {
            format(state, call.invocable);
            format(state, call.arguments);
        }

        void operator()(cst::expression::Tuple_initializer const& initializer)
        {
            format(state, initializer.constructor_path);
            format(state, "(");
            format_comma_separated(state, initializer.initializers.value.elements);
            format(state, ")");
        }

        void operator()(cst::expression::Struct_initializer const& initializer)
        {
            format(state, initializer.constructor_path);
            format(state, " {{ ");
            format_comma_separated(state, initializer.initializers.value.elements);
            format(state, " }}");
        }

        void operator()(cst::expression::Method_call const& call)
        {
            format(state, call.base_expression);
            format(state, ".{}", call.method_name);
            format(state, call.template_arguments);
            format(state, call.function_arguments);
        }

        void operator()(cst::expression::Match const& match)
        {
            format(state, "match ");
            format(state, match.matched_expression);
            format(state, " {{");
            {
                auto const _ = state.indent();
                for (auto const& match_case : match.cases.value) {
                    format(state, "{}", state.newline());
                    format(state, match_case.pattern);
                    format(state, " -> ");
                    format(state, match_case.handler);
                    if (match_case.optional_semicolon_token.has_value()) {
                        format(state, ";");
                    }
                }
            }
            format(state, "{}}}", state.newline());
        }

        void operator()(cst::expression::Sizeof const& sizeof_)
        {
            format(state, "sizeof(");
            format(state, sizeof_.inspected_type.value);
            format(state, ")");
        }

        void operator()(cst::expression::Move const& move)
        {
            format(state, "mv ");
            format(state, move.place_expression);
        }

        void operator()(cst::expression::Type_alias const& alias)
        {
            format(state, "alias {} = ", alias.name);
            format(state, alias.type);
        }

        void operator()(cst::expression::Let const& let)
        {
            format(state, "let ");
            format(state, let.pattern);
            format(state, let.type);
            format(state, " = ");
            format(state, let.initializer);
        }

        void operator()(cst::expression::Array const& array)
        {
            format(state, "[");
            format_comma_separated(state, array.elements.value.elements);
            format(state, "]");
        }

        void operator()(cst::expression::Tuple_field const& field)
        {
            format(state, field.base_expression);
            format(state, ".{}", field.field_index);
        }

        void operator()(cst::expression::Struct_field const& field)
        {
            format(state, field.base_expression);
            format(state, ".{}", field.name);
        }

        void operator()(cst::expression::Array_index const& index)
        {
            format(state, index.base_expression);
            format(state, ".[");
            format(state, index.index_expression.value);
            format(state, "]");
        }

        void operator()(cst::expression::Addressof const& reference)
        {
            format(state, "&");
            format_mutability_with_whitespace(state, reference.mutability);
            format(state, reference.place_expression);
        }

        void operator()(cst::expression::Dereference const& dereference)
        {
            format(state, "*");
            format(state, dereference.reference_expression);
        }

        void operator()(cst::expression::Ascription const& ascription)
        {
            format(state, ascription.base_expression);
            format(state, ": ");
            format(state, ascription.ascribed_type);
        }

        void operator()(cst::expression::For_loop const& loop)
        {
            format(state, "for ");
            format(state, loop.iterator);
            format(state, " in ");
            format(state, loop.iterable);
            format(state, " ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expression::While_loop const& loop)
        {
            format(state, "while ");
            format(state, loop.condition);
            format(state, " ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expression::Loop const& loop)
        {
            format(state, "loop ");
            format_regular_block(as_block(loop.body));
        }

        void operator()(cst::expression::Ret const& ret)
        {
            if (ret.returned_expression.has_value()) {
                format(state, "ret ");
                format(state, ret.returned_expression.value());
            }
            else {
                format(state, "ret");
            }
        }

        void operator()(cst::expression::Conditional const& conditional)
        {
            format(state, "{} ", conditional.is_elif ? "elif" : "if");
            format(state, conditional.condition);
            format(state, " ");
            format_indented_block_body(as_block(conditional.true_branch));
            if (not conditional.false_branch.has_value()) {
                return;
            }
            if (auto const* const else_conditional = std::get_if<cst::expression::Conditional>(
                    &state.arena.expressions[conditional.false_branch.value().body].variant)) {
                if (else_conditional->is_elif) {
                    format(state, "{}", state.newline());
                    format(state, conditional.false_branch.value().body);
                    return;
                }
            }
            format(state, "{}else ", state.newline());
            format_indented_block_body(as_block(conditional.false_branch.value().body));
        }

        void operator()(cst::expression::Break const& break_)
        {
            if (break_.result.has_value()) {
                format(state, "break ");
                format(state, break_.result.value());
            }
            else {
                format(state, "break");
            }
        }

        void operator()(cst::expression::Defer const& defer)
        {
            format(state, "defer ");
            format(state, defer.effect_expression);
        }

        void operator()(cst::expression::Continue const&)
        {
            format(state, "continue");
        }
    };
} // namespace

void libformat::format(State& state, cst::Expression const& expression)
{
    std::visit(Expression_format_visitor { state }, expression.variant);
}
