#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    constexpr std::tuple operator_precedence_table {
        std::to_array<std::string_view>({ "*", "/", "%" }),
        std::to_array<std::string_view>({ "+", "-" }),
        std::to_array<std::string_view>({ "?=", "!=" }),
        std::to_array<std::string_view>({ "<", "<=", ">=", ">" }),
        std::to_array<std::string_view>({ "&&", "||" }),
        std::to_array<std::string_view>({ ":=", "+=", "*=", "/=", "%=" }),
    };
    constexpr std::size_t lowest_operator_precedence
        = -1 + std::tuple_size_v<decltype(operator_precedence_table)>;

    using Operator_and_operand
        = cst::expression::Binary_operator_invocation_sequence::Operator_and_operand;

    template <std::size_t precedence = lowest_operator_precedence>
    auto desugar_binary_operator_invocation_sequence(
        Context&                            context,
        utl::Wrapper<cst::Expression> const leftmost_expression,
        Operator_and_operand const*&        tail_begin,
        Operator_and_operand const* const   tail_end) -> ast::Expression
    {
        // TODO: cleanup
        if constexpr (precedence != static_cast<std::size_t>(-1)) {
            auto const recurse = [&](utl::Wrapper<cst::Expression> const leftmost) {
                return desugar_binary_operator_invocation_sequence<precedence - 1>(
                    context, leftmost, tail_begin, tail_end);
            };
            ast::Expression left = recurse(leftmost_expression);
            while (tail_begin != tail_end) {
                auto const& [right_operand, operator_name] = *tail_begin;
                if constexpr (precedence != lowest_operator_precedence) {
                    static constexpr auto current_operator_group
                        = std::get<precedence>(operator_precedence_table);

                    if (std::ranges::find(
                            current_operator_group, operator_name.operator_id.string.view())
                        == current_operator_group.end())
                    {
                        return left;
                    }
                }
                ++tail_begin;
                auto const source_range = left.source_range.up_to(right_operand->source_range);
                left = ast::Expression {
                    .value = ast::expression::Binary_operator_invocation {
                        .left  = context.wrap(std::move(left)),
                        .right = context.wrap(recurse(right_operand)),
                        .op    = operator_name.operator_id,
                    },
                    .source_range = source_range,
                };
            }
            return left;
        }
        else {
            // precedence == -1
            return context.desugar(*leftmost_expression);
        }
    }

    struct Expression_desugaring_visitor {
        Context&               context;
        cst::Expression const& this_expression;

        template <kieli::literal Literal>
        auto operator()(Literal const literal) -> ast::Expression::Variant
        {
            return literal;
        }

        auto operator()(cst::expression::Parenthesized const& parenthesized)
            -> ast::Expression::Variant
        {
            return std::visit(
                Expression_desugaring_visitor { context, *parenthesized.expression.value },
                parenthesized.expression.value->value);
        }

        auto operator()(cst::expression::Array_literal const& literal) -> ast::Expression::Variant
        {
            return ast::expression::Array_literal {
                .elements
                = std::views::transform(literal.elements.value.elements, context.deref_desugar())
                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::expression::Self const&) -> ast::Expression::Variant
        {
            return ast::expression::Self {};
        }

        auto operator()(cst::expression::Variable const& variable) -> ast::Expression::Variant
        {
            return ast::expression::Variable { .name = context.desugar(variable.name) };
        }

        auto operator()(cst::expression::Tuple const& tuple) -> ast::Expression::Variant
        {
            return ast::expression::Tuple {
                .fields
                = std::views::transform(tuple.fields.value.elements, context.deref_desugar())
                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::expression::Conditional const& conditional) -> ast::Expression::Variant
        {
            utl::wrapper auto const false_branch
                = conditional.false_branch.has_value()
                    ? context.desugar(conditional.false_branch->body)
                    : context.unit_value(this_expression.source_range);

            if (auto const* const let
                = std::get_if<cst::expression::Conditional_let>(&conditional.condition->value))
            {
                /*
                    if let a = b { c } else { d }

                    is transformed into

                    match b {
                        a -> c
                        _ -> d
                    }
                */

                return ast::expression::Match {
                    .cases = utl::to_vector<ast::expression::Match::Case>({
                        {
                            .pattern = context.desugar(let->pattern),
                            .handler = context.desugar(conditional.true_branch),
                        },
                        {
                            .pattern = context.wildcard_pattern(let->pattern->source_range),
                            .handler = false_branch,
                        },
                    }),

                    .matched_expression = context.desugar(let->initializer),
                };
            }

            utl::wrapper auto condition = context.desugar(conditional.condition);
            if (std::holds_alternative<kieli::Boolean>(condition->value)) {
                context.diagnostics().emit(
                    cppdiag::Severity::information,
                    context.source,
                    condition->source_range,
                    "Constant condition");
            }
            return ast::expression::Conditional {
                .condition                 = condition,
                .true_branch               = context.desugar(conditional.true_branch),
                .false_branch              = false_branch,
                .source                    = conditional.is_elif_conditional.get()
                                               ? ast::expression::Conditional::Source::elif_conditional
                                               : ast::expression::Conditional::Source::normal_conditional,
                .has_explicit_false_branch = conditional.false_branch.has_value(),
            };
        }

        auto operator()(cst::expression::Match const& match) -> ast::Expression::Variant
        {
            auto const desugar_match_case = [this](cst::expression::Match::Case const& match_case) {
                return ast::expression::Match::Case {
                    .pattern = context.desugar(match_case.pattern),
                    .handler = context.desugar(match_case.handler),
                };
            };
            return ast::expression::Match {
                .cases = std::views::transform(match.cases.value, desugar_match_case)
                       | std::ranges::to<std::vector>(),
                .matched_expression = context.desugar(match.matched_expression),
            };
        }

        auto operator()(cst::expression::Block const& block) -> ast::Expression::Variant
        {
            std::vector<ast::Expression> side_effects;
            side_effects.reserve(block.side_effects.size());
            for (auto const& side_effect : block.side_effects) {
                side_effects.push_back(context.desugar(*side_effect.expression));
            }
            if (block.result_expression.has_value()) {
                return ast::expression::Block {
                    .side_effect_expressions = std::move(side_effects),
                    .result_expression       = context.desugar(block.result_expression.value()),
                };
            }
            return ast::expression::Block {
                .side_effect_expressions = std::move(side_effects),
                .result_expression       = context.unit_value(block.close_brace_token.source_range),
            };
        }

        auto operator()(cst::expression::While_loop const& loop) -> ast::Expression::Variant
        {
            if (auto const* const let
                = std::get_if<cst::expression::Conditional_let>(&loop.condition->value))
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

                return ast::expression::Loop {
                    .body = context.wrap(ast::Expression {
                        .value = ast::expression::Match {
                            .cases = utl::to_vector<ast::expression::Match::Case>({
                                {
                                    .pattern = context.desugar(let->pattern),
                                    .handler = context.desugar(loop.body),
                                },
                                {
                                    .pattern = context.wildcard_pattern(this_expression.source_range),
                                    .handler = context.wrap(ast::Expression {
                                        .value = ast::expression::Break {
                                            .result = context.unit_value(this_expression.source_range),
                                        },
                                        .source_range = this_expression.source_range,
                                    })
                                }
                            }),
                            .matched_expression = context.desugar(let->initializer),
                        },
                        .source_range = loop.body->source_range,
                    }),
                    .source = ast::expression::Loop::Source::while_loop,
                };
            }

            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */

            utl::wrapper auto const condition = context.desugar(loop.condition);
            if (auto const* const boolean = std::get_if<kieli::Boolean>(&condition->value)) {
                context.diagnostics().emit(
                    cppdiag::Severity::information,
                    context.source,
                    condition->source_range,
                    "Constant condition: {}",
                    boolean->value ? "consider using `loop` instead of `while true`"
                                   : "loop body will never be executed");
            }
            return ast::expression::Loop {
                .body = context.wrap(ast::Expression {
                    .value = ast::expression::Conditional {
                        .condition    = condition,
                        .true_branch  = context.desugar(loop.body),
                        .false_branch = context.wrap(ast::Expression {
                            .value = ast::expression::Break {
                                .result = context.unit_value(this_expression.source_range),
                            },
                            .source_range = this_expression.source_range,
                        }),
                        .source                    = ast::expression::Conditional::Source::while_loop_body,
                        .has_explicit_false_branch = true,
                    },
                    .source_range = loop.body->source_range,
                }),
                .source = ast::expression::Loop::Source::while_loop,
            };
        }

        auto operator()(cst::expression::Infinite_loop const& loop) -> ast::Expression::Variant
        {
            return ast::expression::Loop {
                .body   = context.desugar(loop.body),
                .source = ast::expression::Loop::Source::plain_loop,
            };
        }

        auto operator()(cst::expression::Invocation const& invocation) -> ast::Expression::Variant
        {
            return ast::expression::Invocation {
                .arguments = context.desugar(invocation.function_arguments.value.elements),
                .invocable = context.desugar(invocation.function_expression),
            };
        }

        auto operator()(cst::expression::Struct_initializer const& cst_struct_initializer)
            -> ast::Expression::Variant
        {
            ast::expression::Struct_initializer ast_struct_initializer {
                .struct_type = context.desugar(cst_struct_initializer.struct_type),
            };
            auto const& initializers = cst_struct_initializer.member_initializers.value.elements;
            for (auto it = initializers.begin(); it != initializers.end(); ++it) {
                static constexpr auto projection
                    = &cst::expression::Struct_initializer::Member_initializer::name;
                if (auto duplicate
                    = std::ranges::find(it + 1, initializers.end(), it->name, projection);
                    duplicate != initializers.end())
                {
                    context.diagnostics().error(
                        {
                            { context.source,
                              it->name.source_range,
                              "First specified here",
                              cppdiag::Severity::information },
                            { context.source,
                              duplicate->name.source_range,
                              "Later specified here" },
                        },
                        "Struct initializer contains more than one initializer for member {}",
                        it->name);
                }
                ast_struct_initializer.member_initializers.add_new_unchecked(
                    it->name, context.desugar(it->expression));
            }
            return ast_struct_initializer;
        }

        auto operator()(cst::expression::Binary_operator_invocation_sequence const& sequence)
            -> ast::Expression::Variant
        {
            Operator_and_operand const*       tail_begin = sequence.sequence_tail.data();
            Operator_and_operand const* const tail_end = tail_begin + sequence.sequence_tail.size();
            return desugar_binary_operator_invocation_sequence(
                       context, sequence.leftmost_operand, tail_begin, tail_end)
                .value;
        }

        auto operator()(cst::expression::Template_application const& application)
            -> ast::Expression::Variant
        {
            return ast::expression::Template_application {
                .template_arguments
                = context.desugar(application.template_arguments.value.elements),
                .name = context.desugar(application.name),
            };
        }

        auto operator()(cst::expression::Struct_field_access const& access)
            -> ast::Expression::Variant
        {
            return ast::expression::Struct_field_access {
                .base_expression = context.desugar(access.base_expression),
                .field_name      = access.field_name,
            };
        }

        auto operator()(cst::expression::Tuple_field_access const& access)
            -> ast::Expression::Variant
        {
            return ast::expression::Tuple_field_access {
                .base_expression          = context.desugar(access.base_expression),
                .field_index              = access.field_index,
                .field_index_source_range = access.field_index_token.source_range,
            };
        }

        auto operator()(cst::expression::Array_index_access const& access)
            -> ast::Expression::Variant
        {
            return ast::expression::Array_index_access {
                .base_expression  = context.desugar(access.base_expression),
                .index_expression = context.desugar(access.index_expression),
            };
        }

        auto operator()(cst::expression::Method_invocation const& invocation)
            -> ast::Expression::Variant
        {
            return ast::expression::Method_invocation {
                .function_arguments = context.desugar(invocation.function_arguments.value.elements),
                .template_arguments = invocation.template_arguments.transform(context.desugar()),
                .base_expression    = context.desugar(invocation.base_expression),
                .method_name        = invocation.method_name,
            };
        }

        auto operator()(cst::expression::Type_cast const& cast) -> ast::Expression::Variant
        {
            return ast::expression::Type_cast {
                .expression  = context.desugar(cast.base_expression),
                .target_type = context.desugar(cast.target_type),
            };
        }

        auto operator()(cst::expression::Type_ascription const& cast) -> ast::Expression::Variant
        {
            return ast::expression::Type_ascription {
                .expression    = context.desugar(cast.base_expression),
                .ascribed_type = context.desugar(cast.ascribed_type),
            };
        }

        auto operator()(cst::expression::Let_binding const& let) -> ast::Expression::Variant
        {
            return ast::expression::Let_binding {
                .pattern     = context.desugar(let.pattern),
                .initializer = context.desugar(let.initializer),
                .type        = let.type.transform(context.wrap_desugar()),
            };
        }

        auto operator()(cst::expression::Local_type_alias const& alias) -> ast::Expression::Variant
        {
            return ast::expression::Local_type_alias {
                .alias_name   = alias.alias_name,
                .aliased_type = context.desugar(alias.aliased_type),
            };
        }

        auto operator()(cst::expression::Ret const& ret) -> ast::Expression::Variant
        {
            return ast::expression::Ret {
                .returned_expression = ret.returned_expression.transform(context.desugar()),
            };
        }

        auto operator()(cst::expression::Discard const& discard) -> ast::Expression::Variant
        {
            /*
                discard x

                is transformed into

                { let _ = x; () }
            */

            return ast::expression::Block {
                .side_effect_expressions = utl::to_vector({
                    ast::Expression {
                        .value = ast::expression::Let_binding {
                            .pattern     = context.wildcard_pattern(this_expression.source_range),
                            .initializer = context.desugar(discard.discarded_expression),
                            .type        = std::nullopt,
                        },
                        .source_range = this_expression.source_range,
                    }
                }),
                .result_expression = context.unit_value(this_expression.source_range),
            };
        }

        auto operator()(cst::expression::Break const& break_) -> ast::Expression::Variant
        {
            return ast::expression::Break {
                .result = break_.result.has_value()
                            ? context.desugar(break_.result.value())
                            : context.unit_value(this_expression.source_range),
            };
        }

        auto operator()(cst::expression::Continue const&) -> ast::Expression::Variant
        {
            return ast::expression::Continue {};
        }

        auto operator()(cst::expression::Sizeof const& sizeof_) -> ast::Expression::Variant
        {
            return ast::expression::Sizeof {
                .inspected_type = context.desugar(sizeof_.inspected_type.value),
            };
        }

        auto operator()(cst::expression::Reference const& reference) -> ast::Expression::Variant
        {
            return ast::expression::Reference {
                .mutability = context.desugar_mutability(
                    reference.mutability, reference.ampersand_token.source_range),
                .referenced_expression = context.desugar(reference.referenced_expression),
            };
        }

        auto operator()(cst::expression::Addressof const& addressof) -> ast::Expression::Variant
        {
            return ast::expression::Addressof {
                .lvalue_expression = context.desugar(addressof.lvalue_expression.value),
            };
        }

        auto operator()(cst::expression::Reference_dereference const& dereference)
            -> ast::Expression::Variant
        {
            return ast::expression::Reference_dereference {
                .dereferenced_expression = context.desugar(dereference.dereferenced_expression),
            };
        }

        auto operator()(cst::expression::Pointer_dereference const& dereference)
            -> ast::Expression::Variant
        {
            return ast::expression::Pointer_dereference {
                .pointer_expression = context.desugar(dereference.pointer_expression.value),
            };
        }

        auto operator()(cst::expression::Unsafe const& unsafe) -> ast::Expression::Variant
        {
            return ast::expression::Unsafe { .expression = context.desugar(unsafe.expression) };
        }

        auto operator()(cst::expression::Move const& move) -> ast::Expression::Variant
        {
            return ast::expression::Move { .lvalue = context.desugar(move.lvalue) };
        }

        auto operator()(cst::expression::Meta const& meta) -> ast::Expression::Variant
        {
            return ast::expression::Meta { .expression = context.desugar(meta.expression.value) };
        }

        auto operator()(cst::expression::Hole const&) -> ast::Expression::Variant
        {
            return ast::expression::Hole {};
        }

        auto operator()(cst::expression::For_loop const&) -> ast::Expression::Variant
        {
            context.diagnostics().error(
                context.source, this_expression.source_range, "For loops are not supported yet");
        }

        auto operator()(cst::expression::Conditional_let const&) -> ast::Expression::Variant
        {
            // Should be unreachable because a conditional let expression can
            // only occur as the condition of if-let or while-let expressions.
            cpputil::unreachable();
        }
    };
} // namespace

auto libdesugar::Context::desugar(cst::Expression const& expression) -> ast::Expression
{
    return {
        .value = std::visit(Expression_desugaring_visitor { *this, expression }, expression.value),
        .source_range = expression.source_range,
    };
}
