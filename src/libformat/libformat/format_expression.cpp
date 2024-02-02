#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>

namespace {
    struct Expression_format_visitor {
        libformat::State& state;

        auto format_indented_block_body(cst::expression::Block const& block)
        {
            state.format("{{");
            {
                auto const _ = state.indent();
                for (auto const& side_effect : block.side_effects) {
                    state.format("{}", state.newline());
                    state.format(side_effect.expression);
                    state.format(";");
                }
                if (block.result_expression.has_value()) {
                    state.format("{}", state.newline());
                    state.format(block.result_expression.value());
                }
            }
            state.format("{}}}", state.newline());
        }

        auto format_regular_block(cst::expression::Block const& block)
        {
            if (block.side_effects.empty()) {
                if (block.result_expression.has_value()) {
                    state.format("{{ ");
                    state.format(block.result_expression.value());
                    state.format(" }}");
                }
                else {
                    state.format("{{}}");
                }
            }
            else {
                format_indented_block_body(block);
            }
        }

        static auto as_block(utl::Wrapper<cst::Expression> const expression)
            -> cst::expression::Block const&
        {
            auto const* const block = std::get_if<cst::expression::Block>(&expression->value);
            cpputil::always_assert(block != nullptr);
            return *block;
        }

        template <kieli::literal Literal>
        auto operator()(Literal const& literal)
        {
            if constexpr (std::is_same_v<Literal, kieli::String>) {
                state.format("\"{}\"", literal.value);
            }
            else if constexpr (std::is_same_v<Literal, kieli::Character>) {
                state.format("'{}'", literal.value);
            }
            else {
                state.format("{}", literal.value);
            }
        }

        auto operator()(cst::expression::Block const& block)
        {
            format_regular_block(block);
        }

        auto operator()(cst::expression::Parenthesized const& parenthesized)
        {
            state.format("(");
            state.format(parenthesized.expression.value);
            state.format(")");
        }

        auto operator()(cst::expression::Tuple const& tuple)
        {
            state.format("(");
            state.format_comma_separated(tuple.fields.value.elements);
            state.format(")");
        }

        auto operator()(cst::expression::Binary_operator_invocation_sequence const& sequence)
        {
            state.format(*sequence.leftmost_operand);
            for (auto const& [right_operand, operator_name] : sequence.sequence_tail) {
                state.format(" {} ", operator_name.operator_id);
                state.format(*right_operand);
            }
        }

        auto operator()(cst::expression::Conditional_let const& let)
        {
            state.format("let ");
            state.format(let.pattern);
            state.format(" = ");
            state.format(let.initializer);
        }

        auto operator()(cst::expression::Invocation const& invocation)
        {
            state.format(invocation.function_expression);
            state.format(invocation.function_arguments);
        }

        auto operator()(cst::expression::Unit_initializer const& initializer)
        {
            state.format(initializer.constructor);
        }

        auto operator()(cst::expression::Tuple_initializer const& initializer)
        {
            state.format(initializer.constructor);
            state.format("(");
            state.format_comma_separated(initializer.initializers.value.elements);
            state.format(")");
        }

        auto operator()(cst::expression::Struct_initializer const& initializer)
        {
            state.format(initializer.constructor);
            state.format(" {{ ");
            state.format_comma_separated(initializer.initializers.value.elements);
            state.format(" }}");
        }

        auto operator()(cst::expression::Method_invocation const& invocation)
        {
            state.format(invocation.base_expression);
            state.format(".{}", invocation.method_name);
            state.format(invocation.template_arguments);
            state.format(invocation.function_arguments);
        }

        auto operator()(cst::expression::Match const& match)
        {
            state.format("match ");
            state.format(match.matched_expression);
            state.format(" {{");
            {
                // TODO: align arrows
                auto const _ = state.indent();
                for (auto const& match_case : match.cases.value) {
                    state.format("{}", state.newline());
                    state.format(match_case.pattern);
                    state.format(" -> ");
                    state.format(match_case.handler);
                    if (match_case.optional_semicolon_token.has_value()) {
                        state.format(";");
                    }
                }
            }
            state.format("{}}}", state.newline());
        }

        auto operator()(cst::expression::Variable const& variable)
        {
            state.format(variable.name);
        }

        auto operator()(cst::expression::Unsafe const& unsafe)
        {
            state.format("unsafe ");
            state.format(unsafe.expression);
        }

        auto operator()(cst::expression::Sizeof const& sizeof_)
        {
            state.format("sizeof(");
            state.format(sizeof_.inspected_type.value);
            state.format(")");
        }

        auto operator()(cst::expression::Move const& move)
        {
            state.format("mov ");
            state.format(move.lvalue);
        }

        auto operator()(cst::expression::Local_type_alias const& alias)
        {
            state.format("alias {} = ", alias.alias_name);
            state.format(alias.aliased_type);
        }

        auto operator()(cst::expression::Let_binding const& let)
        {
            state.format("let ");
            state.format(let.pattern);
            if (let.type.has_value()) {
                state.format(": ");
                state.format(let.type->type);
            }
            state.format(" = ");
            state.format(let.initializer);
        }

        auto operator()(cst::expression::Array_literal const& array)
        {
            state.format("[");
            state.format_comma_separated(array.elements.value.elements);
            state.format("]");
        }

        auto operator()(cst::expression::Tuple_field_access const& access)
        {
            state.format(access.base_expression);
            state.format(".{}", access.field_index);
        }

        auto operator()(cst::expression::Struct_field_access const& access)
        {
            state.format(access.base_expression);
            state.format(".{}", access.field_name);
        }

        auto operator()(cst::expression::Array_index_access const& access)
        {
            state.format(access.base_expression);
            state.format(".[");
            state.format(access.index_expression.value);
            state.format("]");
        }

        auto operator()(cst::expression::Reference const& reference)
        {
            state.format("&");
            state.format_mutability_with_trailing_whitespace(reference.mutability);
            state.format(reference.referenced_expression);
        }

        auto operator()(cst::expression::Addressof const& addressof)
        {
            state.format("addressof(");
            state.format(addressof.lvalue_expression.value);
            state.format(")");
        }

        auto operator()(cst::expression::Reference_dereference const& dereference)
        {
            state.format("*");
            state.format(dereference.dereferenced_expression);
        }

        auto operator()(cst::expression::Pointer_dereference const& dereference)
        {
            state.format("dereference(");
            state.format(dereference.pointer_expression.value);
            state.format(")");
        }

        auto operator()(cst::expression::Meta const& meta)
        {
            state.format("meta(");
            state.format(meta.expression.value);
            state.format(")");
        }

        auto operator()(cst::expression::Type_cast const& cast)
        {
            state.format(cast.base_expression);
            state.format(" as ");
            state.format(cast.target_type);
        }

        auto operator()(cst::expression::Type_ascription const& ascription)
        {
            state.format(ascription.base_expression);
            state.format(": ");
            state.format(ascription.ascribed_type);
        }

        auto operator()(cst::expression::Template_application const& application)
        {
            state.format(application.name);
            state.format(application.template_arguments);
        }

        auto operator()(cst::expression::Discard const& discard)
        {
            state.format("discard ");
            state.format(discard.discarded_expression);
        }

        auto operator()(cst::expression::For_loop const& loop)
        {
            state.format("for ");
            state.format(loop.iterator);
            state.format(" in ");
            state.format(loop.iterable);
            state.format(" ");
            format_regular_block(as_block(loop.body));
        }

        auto operator()(cst::expression::While_loop const& loop)
        {
            state.format("while ");
            state.format(loop.condition);
            state.format(" ");
            format_regular_block(as_block(loop.body));
        }

        auto operator()(cst::expression::Infinite_loop const& loop)
        {
            state.format("loop ");
            format_regular_block(as_block(loop.body));
        }

        auto operator()(cst::expression::Ret const& ret)
        {
            if (ret.returned_expression.has_value()) {
                state.format("ret ");
                state.format(ret.returned_expression.value());
            }
            else {
                state.format("ret");
            }
        }

        auto operator()(cst::expression::Conditional const& conditional)
        {
            state.format("{} ", conditional.is_elif_conditional.get() ? "elif" : "if");
            state.format(conditional.condition);
            state.format(" ");
            format_indented_block_body(as_block(conditional.true_branch));
            if (!conditional.false_branch.has_value()) {
                return;
            }
            if (auto const* const else_conditional
                = std::get_if<cst::expression::Conditional>(&conditional.false_branch->body->value))
            {
                if (else_conditional->is_elif_conditional.get()) {
                    state.format("{}", state.newline());
                    state.format(conditional.false_branch->body);
                    return;
                }
            }
            state.format("{}else ", state.newline());
            format_indented_block_body(as_block(conditional.false_branch->body));
        }

        auto operator()(cst::expression::Break const& break_)
        {
            if (break_.result.has_value()) {
                state.format("break ");
                state.format(*break_.result.value());
            }
            else {
                state.format("break");
            }
        }

        auto operator()(cst::expression::Continue const&)
        {
            state.format("continue");
        }

        auto operator()(cst::expression::Hole const&)
        {
            state.format(R"(???)");
        }

        auto operator()(cst::expression::Self const&)
        {
            state.format("self");
        }
    };
} // namespace

auto libformat::State::format(cst::Expression const& expression) -> void
{
    std::visit(Expression_format_visitor { *this }, expression.value);
}
