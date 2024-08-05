#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    // TODO: just mark the duplicate as erroneous
    [[noreturn]] auto emit_duplicate_fields_error(
        Context&           context,
        kieli::Lower const name,
        kieli::Range const first,
        kieli::Range const second) -> void
    {
        kieli::Source const& source = context.db.sources[context.source];
        context.db.diagnostics.push_back(cppdiag::Diagnostic {
            .text_sections = utl::to_vector({
                kieli::text_section(
                    source, first, "First specified here", cppdiag::Severity::information),
                kieli::text_section(source, second, "Later specified here"),
            }),
            .message       = std::format("More than one initializer for struct member {}", name),
            .severity      = cppdiag::Severity::error,
        });
        throw kieli::Compilation_failure {};
    }

    auto ensure_no_duplicate_fields(
        Context& context, cst::expression::Struct_initializer const& initializer) -> void
    {
        auto const& initializers = initializer.initializers.value.elements;
        for (auto it = initializers.begin(); it != initializers.end(); ++it) {
            if (auto duplicate = std::ranges::find(
                    it + 1,
                    initializers.end(),
                    it->name,
                    &cst::expression::Struct_initializer::Field::name);
                duplicate != initializers.end()) {
                emit_duplicate_fields_error(
                    context, it->name, it->name.range, duplicate->name.range);
            }
        }
    }

    constexpr auto operators_1 = std::to_array<std::string_view>({ "*", "/", "%" });
    constexpr auto operators_2 = std::to_array<std::string_view>({ "+", "-" });
    constexpr auto operators_3 = std::to_array<std::string_view>({ "?=", "!=" });
    constexpr auto operators_4 = std::to_array<std::string_view>({ "<", "<=", ">=", ">" });
    constexpr auto operators_5 = std::to_array<std::string_view>({ "&&", "||" });
    constexpr auto operators_6 = std::to_array<std::string_view>({ ":=", "+=", "*=", "/=", "%=" });

    constexpr auto operator_precedence_table = std::to_array<std::span<std::string_view const>>({
        operators_1,
        operators_2,
        operators_3,
        operators_4,
        operators_5,
        operators_6,
    });

    constexpr auto lowest_operator_precedence = operator_precedence_table.size() - 1;

    auto desugar_operator_chain(
        Context&                                               context,
        cst::Expression_id const                               leftmost_expression,
        std::size_t const                                      precedence,
        std::span<cst::expression::Operator_chain::Rhs const>& tail) -> ast::Expression
    {
        if (precedence == std::numeric_limits<std::size_t>::max()) {
            return context.deref_desugar(leftmost_expression);
        }

        auto const recurse = [&](cst::Expression_id const leftmost) {
            return desugar_operator_chain(context, leftmost, precedence - 1, tail);
        };

        ast::Expression left = recurse(leftmost_expression);
        while (!tail.empty()) {
            auto const& [right_operand, operator_name] = tail.front();
            if (precedence != lowest_operator_precedence) {
                auto const operators = operator_precedence_table.at(precedence);
                if (!std::ranges::contains(operators, operator_name.identifier.view())) {
                    return left;
                }
            }

            tail = tail.subspan<1>();
            left = ast::Expression {
                ast::expression::Binary_operator_invocation {
                    .left     = context.wrap(std::move(left)),
                    .right    = context.wrap(recurse(right_operand)),
                    .op       = operator_name.identifier,
                    .op_range = operator_name.range,
                },
                kieli::Range(left.range.start, context.cst.expressions[right_operand].range.stop),
            };
        }
        return left;
    }

    auto break_expression(Context& context, kieli::Range const range)
        -> utl::Wrapper<ast::Expression>
    {
        return context.wrap(ast::Expression {
            .variant = ast::expression::Break { context.wrap(unit_value(range)) },
            .range   = range,
        });
    }

    struct Expression_desugaring_visitor {
        Context&               context;
        cst::Expression const& this_expression;

        auto operator()(kieli::literal auto const& literal) -> ast::Expression_variant
        {
            return literal;
        }

        auto operator()(cst::expression::Parenthesized const& parenthesized)
            -> ast::Expression_variant
        {
            cst::Expression const& expression
                = context.cst.expressions[parenthesized.expression.value];
            return std::visit(
                Expression_desugaring_visitor { context, expression }, expression.variant);
        }

        auto operator()(cst::expression::Array_literal const& literal) -> ast::Expression_variant
        {
            return ast::expression::Array_literal {
                .elements = literal.elements.value.elements
                          | std::views::transform(context.deref_desugar())
                          | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::expression::Self const&) -> ast::Expression_variant
        {
            return ast::expression::Self {};
        }

        auto operator()(cst::expression::Variable const& variable) -> ast::Expression_variant
        {
            return ast::expression::Variable { .path = context.desugar(variable.path) };
        }

        auto operator()(cst::expression::Tuple const& tuple) -> ast::Expression_variant
        {
            return ast::expression::Tuple {
                .fields = tuple.fields.value.elements
                        | std::views::transform(context.deref_desugar())
                        | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::expression::Conditional const& conditional) -> ast::Expression_variant
        {
            utl::Wrapper<ast::Expression> const false_branch
                = conditional.false_branch.has_value()
                    ? context.desugar(conditional.false_branch->body)
                    : context.wrap(unit_value(this_expression.range));

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
                        ast::expression::Match::Case {
                            .pattern    = context.desugar(let->pattern),
                            .expression = context.desugar(conditional.true_branch),
                        },
                        ast::expression::Match::Case {
                            .pattern = context.wrap(
                                wildcard_pattern(context.cst.patterns[let->pattern].range)),
                            .expression = false_branch,
                        },
                    }),

                    .expression = context.desugar(let->initializer),
                };
            }

            utl::Wrapper<ast::Expression> const condition = context.desugar(conditional.condition);
            if (std::holds_alternative<kieli::Boolean>(condition->variant)) {
                kieli::emit_diagnostic(
                    cppdiag::Severity::information,
                    context.db,
                    context.source,
                    condition->range,
                    "Constant condition");
            }

            return ast::expression::Conditional {
                .condition    = condition,
                .true_branch  = context.desugar(conditional.true_branch),
                .false_branch = false_branch,
                .source       = conditional.is_elif.get() ? ast::Conditional_source::elif
                                                          : ast::Conditional_source::if_,
                .has_explicit_false_branch = conditional.false_branch.has_value(),
            };
        }

        auto operator()(cst::expression::Match const& match) -> ast::Expression_variant
        {
            auto const desugar_case = [&](cst::expression::Match::Case const& match_case) {
                return ast::expression::Match::Case {
                    .pattern    = context.desugar(match_case.pattern),
                    .expression = context.desugar(match_case.handler),
                };
            };
            return ast::expression::Match {
                .cases = std::views::transform(match.cases.value, desugar_case)
                       | std::ranges::to<std::vector>(),
                .expression = context.desugar(match.matched_expression),
            };
        }

        auto operator()(cst::expression::Block const& block) -> ast::Expression_variant
        {
            std::vector<ast::Expression> side_effects;
            side_effects.reserve(block.side_effects.size());
            for (auto const& side_effect : block.side_effects) {
                side_effects.push_back(context.deref_desugar(side_effect.expression));
            }
            if (block.result_expression.has_value()) {
                return ast::expression::Block {
                    .side_effects = std::move(side_effects),
                    .result       = context.desugar(block.result_expression.value()),
                };
            }
            return ast::expression::Block {
                .side_effects = std::move(side_effects),
                .result       = context.wrap(unit_value(block.close_brace_token.range)),
            };
        }

        auto desugar_while_let_loop(
            cst::expression::While_loop const&      loop,
            cst::expression::Conditional_let const& let) -> ast::Expression_variant
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
                    ast::expression::Match {
                        .cases      = utl::to_vector({
                            ast::expression::Match::Case {
                                context.desugar(let.pattern),
                                context.desugar(loop.body),
                            },
                            ast::expression::Match::Case {
                                context.wrap(wildcard_pattern(this_expression.range)),
                                break_expression(context, this_expression.range),
                            },
                        }),
                        .expression = context.desugar(let.initializer),
                    },
                    context.cst.expressions[loop.body].range,
                }),

                .source = ast::Loop_source::while_loop,
            };
        }

        auto desugar_while_loop(cst::expression::While_loop const& loop) -> ast::Expression_variant
        {
            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */

            auto const condition = context.desugar(loop.condition);
            if (auto const* const boolean = std::get_if<kieli::Boolean>(&condition->variant)) {
                kieli::emit_diagnostic(
                    cppdiag::Severity::information,
                    context.db,
                    context.source,
                    condition->range,
                    "Constant condition",
                    boolean->value ? "Consider using `loop` instead of `while true`"
                                   : "The loop body will never be executed");
            }
            return ast::expression::Loop {
                .body = context.wrap(ast::Expression {
                    .variant = ast::expression::Conditional {
                        .condition    = condition,
                        .true_branch  = context.desugar(loop.body),
                        .false_branch = break_expression(context, this_expression.range),
                        .source                    = ast::Conditional_source::while_loop_body,
                        .has_explicit_false_branch = true,
                    },
                    .range = context.cst.expressions[loop.body].range,
                }),
                .source = ast::Loop_source::while_loop,
            };
        }

        auto operator()(cst::expression::While_loop const& loop) -> ast::Expression_variant
        {
            if (auto const* const let = std::get_if<cst::expression::Conditional_let>(
                    &context.cst.expressions[loop.condition].variant)) {
                return desugar_while_let_loop(loop, *let);
            }
            return desugar_while_loop(loop);
        }

        auto operator()(cst::expression::Infinite_loop const& loop) -> ast::Expression_variant
        {
            return ast::expression::Loop {
                .body   = context.desugar(loop.body),
                .source = ast::Loop_source::plain_loop,
            };
        }

        auto operator()(cst::expression::Invocation const& invocation) -> ast::Expression_variant
        {
            return ast::expression::Invocation {
                .arguments = context.desugar(invocation.function_arguments.value.elements),
                .invocable = context.desugar(invocation.function_expression),
            };
        }

        auto operator()(cst::expression::Unit_initializer const& initializer)
            -> ast::Expression_variant
        {
            return ast::expression::Unit_initializer {
                .constructor_path = context.desugar(initializer.constructor_path),
            };
        }

        auto operator()(cst::expression::Tuple_initializer const& initializer)
            -> ast::Expression_variant
        {
            return ast::expression::Tuple_initializer {
                .constructor_path = context.desugar(initializer.constructor_path),
                .initializers     = context.desugar(initializer.initializers),
            };
        }

        auto operator()(cst::expression::Struct_initializer const& initializer)
            -> ast::Expression_variant
        {
            ensure_no_duplicate_fields(context, initializer);
            return ast::expression::Struct_initializer {
                .constructor_path = context.desugar(initializer.constructor_path),
                .initializers     = context.desugar(initializer.initializers),
            };
        }

        auto operator()(cst::expression::Operator_chain const& chain) -> ast::Expression_variant
        {
            std::span   tail       = chain.tail;
            std::size_t precedence = lowest_operator_precedence;
            return desugar_operator_chain(context, chain.lhs, precedence, tail).variant;
        }

        auto operator()(cst::expression::Template_application const& application)
            -> ast::Expression_variant
        {
            return ast::expression::Template_application {
                .template_arguments
                = context.desugar(application.template_arguments.value.elements),
                .path = context.desugar(application.path),
            };
        }

        auto operator()(cst::expression::Struct_field_access const& access)
            -> ast::Expression_variant
        {
            return ast::expression::Struct_field_access {
                .base_expression = context.desugar(access.base_expression),
                .field_name      = access.field_name,
            };
        }

        auto operator()(cst::expression::Tuple_field_access const& access)
            -> ast::Expression_variant
        {
            return ast::expression::Tuple_field_access {
                .base_expression   = context.desugar(access.base_expression),
                .field_index       = access.field_index,
                .field_index_range = access.field_index_token.range,
            };
        }

        auto operator()(cst::expression::Array_index_access const& access)
            -> ast::Expression_variant
        {
            return ast::expression::Array_index_access {
                .base_expression  = context.desugar(access.base_expression),
                .index_expression = context.desugar(access.index_expression),
            };
        }

        auto operator()(cst::expression::Method_invocation const& invocation)
            -> ast::Expression_variant
        {
            return ast::expression::Method_invocation {
                .function_arguments = context.desugar(invocation.function_arguments.value.elements),
                .template_arguments = invocation.template_arguments.transform(context.desugar()),
                .base_expression    = context.desugar(invocation.base_expression),
                .method_name        = invocation.method_name,
            };
        }

        auto operator()(cst::expression::Type_cast const& cast) -> ast::Expression_variant
        {
            return ast::expression::Type_cast {
                .expression  = context.desugar(cast.base_expression),
                .target_type = context.desugar(cast.target_type),
            };
        }

        auto operator()(cst::expression::Type_ascription const& cast) -> ast::Expression_variant
        {
            return ast::expression::Type_ascription {
                .expression    = context.desugar(cast.base_expression),
                .ascribed_type = context.desugar(cast.ascribed_type),
            };
        }

        auto operator()(cst::expression::Let_binding const& let) -> ast::Expression_variant
        {
            return ast::expression::Let_binding {
                .pattern     = context.desugar(let.pattern),
                .initializer = context.desugar(let.initializer),
                .type        = let.type.transform(context.wrap_desugar()),
            };
        }

        auto operator()(cst::expression::Local_type_alias const& alias) -> ast::Expression_variant
        {
            return ast::expression::Local_type_alias {
                .name = alias.name,
                .type = context.desugar(alias.type),
            };
        }

        auto operator()(cst::expression::Ret const& ret) -> ast::Expression_variant
        {
            return ast::expression::Ret { ret.returned_expression.transform(context.desugar()) };
        }

        auto operator()(cst::expression::Discard const& discard) -> ast::Expression_variant
        {
            /*
                discard x

                is transformed into

                { let _ = x; () }
            */

            return ast::expression::Block {
                .side_effects = utl::to_vector({
                    ast::Expression {
                        ast::expression::Let_binding {
                            .pattern     = context.wrap(wildcard_pattern(this_expression.range)),
                            .initializer = context.desugar(discard.discarded_expression),
                            .type        = std::nullopt,
                        },
                        this_expression.range,
                    },
                }),
                .result       = context.wrap(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Break const& break_expression) -> ast::Expression_variant
        {
            return ast::expression::Break {
                break_expression.result.has_value()
                    ? context.desugar(break_expression.result.value())
                    : context.wrap(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Continue const&) -> ast::Expression_variant
        {
            return ast::expression::Continue {};
        }

        auto operator()(cst::expression::Sizeof const& sizeof_) -> ast::Expression_variant
        {
            return ast::expression::Sizeof { context.desugar(sizeof_.inspected_type.value) };
        }

        auto operator()(cst::expression::Addressof const& addressof) -> ast::Expression_variant
        {
            return ast::expression::Addressof {
                .mutability
                = context.desugar_mutability(addressof.mutability, addressof.ampersand_token.range),
                .place_expression = context.desugar(addressof.place_expression),
            };
        }

        auto operator()(cst::expression::Dereference const& dereference) -> ast::Expression_variant
        {
            return ast::expression::Dereference {
                .reference_expression = context.desugar(dereference.reference_expression),
            };
        }

        auto operator()(cst::expression::Unsafe const& unsafe) -> ast::Expression_variant
        {
            return ast::expression::Unsafe { .expression = context.desugar(unsafe.expression) };
        }

        auto operator()(cst::expression::Move const& move) -> ast::Expression_variant
        {
            return ast::expression::Move { context.desugar(move.place_expression) };
        }

        auto operator()(cst::expression::Defer const& defer) -> ast::Expression_variant
        {
            return ast::expression::Defer { context.desugar(defer.expression) };
        }

        auto operator()(cst::expression::Meta const& meta) -> ast::Expression_variant
        {
            return ast::expression::Meta { .expression = context.desugar(meta.expression.value) };
        }

        auto operator()(cst::expression::Hole const&) -> ast::Expression_variant
        {
            return ast::expression::Hole {};
        }

        auto operator()(cst::expression::For_loop const&) -> ast::Expression_variant
        {
            kieli::fatal_error(
                context.db,
                context.source,
                this_expression.range,
                "For loops are not supported yet");
        }

        auto operator()(cst::expression::Conditional_let const&) -> ast::Expression_variant
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
        std::visit(Expression_desugaring_visitor { *this, expression }, expression.variant),
        expression.range,
    };
}
