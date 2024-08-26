#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

using namespace libformat;

namespace {
    struct Expression_format_visitor {
        State& state;

        auto format_indented_block_body(cst::expression::Block const& block)
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

        auto format_regular_block(cst::expression::Block const& block)
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
        auto operator()(Literal const& literal)
        {
            if constexpr (std::is_same_v<Literal, kieli::String>) {
                format(state, "\"{}\"", literal.value);
            }
            else if constexpr (std::is_same_v<Literal, kieli::Character>) {
                format(state, "'{}'", literal.value);
            }
            else {
                format(state, "{}", literal.value);
            }
        }

        auto operator()(cst::expression::Block const& block)
        {
            format_regular_block(block);
        }

        auto operator()(cst::expression::Parenthesized const& parenthesized)
        {
            format(state, "(");
            format(state, parenthesized.expression.value);
            format(state, ")");
        }

        auto operator()(cst::expression::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.fields.value.elements);
            format(state, ")");
        }

        auto operator()(cst::expression::Operator_chain const& sequence)
        {
            format(state, sequence.lhs);
            for (auto const& [rhs, op] : sequence.tail) {
                format(state, " {} ", op.identifier);
                format(state, rhs);
            }
        }

        auto operator()(cst::expression::Conditional_let const& let)
        {
            format(state, "let ");
            format(state, let.pattern);
            format(state, " = ");
            format(state, let.initializer);
        }

        auto operator()(cst::expression::Function_call const& call)
        {
            format(state, call.invocable);
            format(state, call.arguments);
        }

        auto operator()(cst::expression::Unit_initializer const& initializer)
        {
            format(state, initializer.constructor_path);
        }

        auto operator()(cst::expression::Tuple_initializer const& initializer)
        {
            format(state, initializer.constructor_path);
            format(state, "(");
            format_comma_separated(state, initializer.initializers.value.elements);
            format(state, ")");
        }

        auto operator()(cst::expression::Struct_initializer const& initializer)
        {
            format(state, initializer.constructor_path);
            format(state, " {{ ");
            format_comma_separated(state, initializer.initializers.value.elements);
            format(state, " }}");
        }

        auto operator()(cst::expression::Method_call const& call)
        {
            format(state, call.base_expression);
            format(state, ".{}", call.method_name);
            format(state, call.template_arguments);
            format(state, call.function_arguments);
        }

        auto operator()(cst::expression::Match const& match)
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

        auto operator()(cst::expression::Variable const& variable)
        {
            format(state, variable.path);
        }

        auto operator()(cst::expression::Unsafe const& unsafe)
        {
            format(state, "unsafe ");
            format(state, unsafe.expression);
        }

        auto operator()(cst::expression::Sizeof const& sizeof_)
        {
            format(state, "sizeof(");
            format(state, sizeof_.inspected_type.value);
            format(state, ")");
        }

        auto operator()(cst::expression::Move const& move)
        {
            format(state, "mov ");
            format(state, move.place_expression);
        }

        auto operator()(cst::expression::Local_type_alias const& alias)
        {
            format(state, "alias {} = ", alias.name);
            format(state, alias.type);
        }

        auto operator()(cst::expression::Let_binding const& let)
        {
            format(state, "let ");
            format(state, let.pattern);
            format(state, let.type);
            format(state, " = ");
            format(state, let.initializer);
        }

        auto operator()(cst::expression::Array_literal const& array)
        {
            format(state, "[");
            format_comma_separated(state, array.elements.value.elements);
            format(state, "]");
        }

        auto operator()(cst::expression::Tuple_field_access const& access)
        {
            format(state, access.base_expression);
            format(state, ".{}", access.field_index);
        }

        auto operator()(cst::expression::Struct_field_access const& access)
        {
            format(state, access.base_expression);
            format(state, ".{}", access.field_name);
        }

        auto operator()(cst::expression::Array_index_access const& access)
        {
            format(state, access.base_expression);
            format(state, ".[");
            format(state, access.index_expression.value);
            format(state, "]");
        }

        auto operator()(cst::expression::Addressof const& reference)
        {
            format(state, "&");
            format_mutability_with_whitespace(state, reference.mutability);
            format(state, reference.place_expression);
        }

        auto operator()(cst::expression::Dereference const& dereference)
        {
            format(state, "*");
            format(state, dereference.reference_expression);
        }

        auto operator()(cst::expression::Meta const& meta)
        {
            format(state, "meta(");
            format(state, meta.expression.value);
            format(state, ")");
        }

        auto operator()(cst::expression::Type_cast const& cast)
        {
            format(state, cast.base_expression);
            format(state, " as ");
            format(state, cast.target_type);
        }

        auto operator()(cst::expression::Type_ascription const& ascription)
        {
            format(state, ascription.base_expression);
            format(state, ": ");
            format(state, ascription.ascribed_type);
        }

        auto operator()(cst::expression::Template_application const& application)
        {
            format(state, application.path);
            format(state, application.template_arguments);
        }

        auto operator()(cst::expression::Discard const& discard)
        {
            format(state, "discard ");
            format(state, discard.discarded_expression);
        }

        auto operator()(cst::expression::For_loop const& loop)
        {
            format(state, "for ");
            format(state, loop.iterator);
            format(state, " in ");
            format(state, loop.iterable);
            format(state, " ");
            format_regular_block(as_block(loop.body));
        }

        auto operator()(cst::expression::While_loop const& loop)
        {
            format(state, "while ");
            format(state, loop.condition);
            format(state, " ");
            format_regular_block(as_block(loop.body));
        }

        auto operator()(cst::expression::Plain_loop const& loop)
        {
            format(state, "loop ");
            format_regular_block(as_block(loop.body));
        }

        auto operator()(cst::expression::Ret const& ret)
        {
            if (ret.returned_expression.has_value()) {
                format(state, "ret ");
                format(state, ret.returned_expression.value());
            }
            else {
                format(state, "ret");
            }
        }

        auto operator()(cst::expression::Conditional const& conditional)
        {
            format(state, "{} ", conditional.is_elif.get() ? "elif" : "if");
            format(state, conditional.condition);
            format(state, " ");
            format_indented_block_body(as_block(conditional.true_branch));
            if (!conditional.false_branch.has_value()) {
                return;
            }
            if (auto const* const else_conditional = std::get_if<cst::expression::Conditional>(
                    &state.arena.expressions[conditional.false_branch.value().body].variant)) {
                if (else_conditional->is_elif.get()) {
                    format(state, "{}", state.newline());
                    format(state, conditional.false_branch.value().body);
                    return;
                }
            }
            format(state, "{}else ", state.newline());
            format_indented_block_body(as_block(conditional.false_branch.value().body));
        }

        auto operator()(cst::expression::Break const& break_)
        {
            if (break_.result.has_value()) {
                format(state, "break ");
                format(state, break_.result.value());
            }
            else {
                format(state, "break");
            }
        }

        auto operator()(cst::expression::Defer const& defer)
        {
            format(state, "defer ");
            format(state, defer.effect_expression);
        }

        auto operator()(cst::expression::Continue const&)
        {
            format(state, "continue");
        }

        auto operator()(cst::expression::Hole const&)
        {
            format(state, R"(???)");
        }
    };
} // namespace

auto libformat::format(State& state, cst::Expression const& expression) -> void
{
    std::visit(Expression_format_visitor { state }, expression.variant);
}
