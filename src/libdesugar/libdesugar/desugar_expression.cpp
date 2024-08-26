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
        Context&                context,
        kieli::Identifier const identifier,
        kieli::Range const      first,
        kieli::Range const      second) -> kieli::Diagnostic
    {
        return kieli::Diagnostic {
            .message      = std::format("Duplicate initializer for struct member {}", identifier),
            .range        = second,
            .severity     = kieli::Severity::error,
            .related_info = utl::to_vector({
                kieli::Diagnostic_related_info {
                    .message = "First specified here",
                    .location { .document_id = context.document_id, .range = first },
                },
            }),
        };
    }

    auto check_has_duplicate_fields(
        Context& context, cst::expression::Struct_initializer const& initializer) -> bool
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
            auto const& [rhs, op] = tail.front();
            if (precedence != lowest_operator_precedence) {
                auto const operators = operator_precedence_table.at(precedence);
                if (!std::ranges::contains(operators, op.identifier.view())) {
                    return left;
                }
            }
            tail = tail.subspan<1>();
            left = ast::Expression {
                ast::expression::Binary_operator_application {
                    .left     = context.ast.expressions.push(std::move(left)),
                    .right    = context.ast.expressions.push(recurse(rhs)),
                    .op       = op.identifier,
                    .op_range = op.range,
                },
                kieli::Range(left.range.start, context.cst.expressions[rhs].range.stop),
            };
        }
        return left;
    }

    auto break_expression(Context& context, kieli::Range const range) -> ast::Expression_id
    {
        return context.ast.expressions.push(
            ast::expression::Break { context.ast.expressions.push(unit_value(range)) }, range);
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
            ast::Expression_id const false_branch
                = conditional.false_branch.has_value()
                    ? context.desugar(conditional.false_branch->body)
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
                            .pattern    = context.desugar(let->pattern),
                            .expression = context.desugar(conditional.true_branch),
                        },
                        ast::Match_case {
                            .pattern = context.ast.patterns.push(
                                wildcard_pattern(context.cst.patterns[let->pattern].range)),
                            .expression = false_branch,
                        },
                    }),

                    .expression = context.desugar(let->initializer),
                };
            }

            ast::Expression condition = context.deref_desugar(conditional.condition);

            if (std::holds_alternative<kieli::Boolean>(condition.variant)) {
                kieli::Diagnostic diagnostic {
                    .message  = "Constant condition",
                    .range    = condition.range,
                    .severity = kieli::Severity::information,
                };
                kieli::add_diagnostic(context.db, context.document_id, std::move(diagnostic));
            }

            return ast::expression::Conditional {
                .condition    = context.ast.expressions.push(std::move(condition)),
                .true_branch  = context.desugar(conditional.true_branch),
                .false_branch = false_branch,
                .source       = conditional.is_elif.get() ? ast::Conditional_source::elif
                                                          : ast::Conditional_source::if_,
                .has_explicit_false_branch = conditional.false_branch.has_value(),
            };
        }

        auto operator()(cst::expression::Match const& match) -> ast::Expression_variant
        {
            auto const desugar_case = [&](cst::Match_case const& match_case) {
                return ast::Match_case {
                    .pattern    = context.desugar(match_case.pattern),
                    .expression = context.desugar(match_case.handler),
                };
            };
            return ast::expression::Match {
                .cases = std::ranges::to<std::vector>(
                    std::views::transform(match.cases.value, desugar_case)),
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

            cst::Token_id const unit_token = //
                block.side_effects.empty()   //
                    ? block.close_brace_token
                    : block.side_effects.back().trailing_semicolon_token;

            ast::Expression_id const result = //
                block.result_expression.has_value()
                    ? context.desugar(block.result_expression.value())
                    : context.ast.expressions.push(
                          unit_value(context.cst.tokens[unit_token].range));

            return ast::expression::Block {
                .side_effects = std::move(side_effects),
                .result       = result,
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
            ast::expression::Match match {
                .cases      = utl::to_vector({
                    ast::Match_case {
                        context.desugar(let.pattern),
                        context.desugar(loop.body),
                    },
                    ast::Match_case {
                        context.ast.patterns.push(wildcard_pattern(this_expression.range)),
                        break_expression(context, this_expression.range),
                    },
                }),
                .expression = context.desugar(let.initializer),
            };
            return ast::expression::Loop {
                .body = context.ast.expressions.push(
                    std::move(match), context.cst.expressions[loop.body].range),
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
            ast::Expression condition = context.deref_desugar(loop.condition);
            if (auto const* const boolean = std::get_if<kieli::Boolean>(&condition.variant)) {
                kieli::add_diagnostic(
                    context.db,
                    context.document_id,
                    constant_loop_condition_diagnostic(condition.range, boolean->value));
            }
            ast::expression::Conditional conditional {
                .condition                 = context.ast.expressions.push(std::move(condition)),
                .true_branch               = context.desugar(loop.body),
                .false_branch              = break_expression(context, this_expression.range),
                .source                    = ast::Conditional_source::while_loop_body,
                .has_explicit_false_branch = true,
            };
            return ast::expression::Loop {
                .body = context.ast.expressions.push(
                    std::move(conditional), context.cst.expressions[loop.body].range),
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

        auto operator()(cst::expression::Plain_loop const& loop) -> ast::Expression_variant
        {
            return ast::expression::Loop {
                .body   = context.desugar(loop.body),
                .source = ast::Loop_source::plain_loop,
            };
        }

        auto operator()(cst::expression::Function_call const& call) -> ast::Expression_variant
        {
            return ast::expression::Function_call {
                .arguments = context.desugar(call.arguments.value.elements),
                .invocable = context.desugar(call.invocable),
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
            if (check_has_duplicate_fields(context, initializer)) {
                return ast::expression::Error {};
            }
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
                .field_index_range = context.cst.tokens[access.field_index_token].range,
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

        auto operator()(cst::expression::Method_call const& call) -> ast::Expression_variant
        {
            return ast::expression::Method_call {
                .function_arguments = context.desugar(call.function_arguments.value.elements),
                .template_arguments = call.template_arguments.transform(context.desugar()),
                .base_expression    = context.desugar(call.base_expression),
                .method_name        = call.method_name,
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
                .type        = let.type.has_value()
                                 ? context.desugar(let.type.value())
                                 : context.ast.types.push(wildcard_type(this_expression.range)),
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
            return ast::expression::Ret {
                ret.returned_expression.has_value()
                    ? context.desugar(ret.returned_expression.value())
                    : context.ast.expressions.push(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Discard const& discard) -> ast::Expression_variant
        {
            /*
                discard x

                is transformed into

                { let _: _ = x; () }
            */
            ast::expression::Let_binding let {
                .pattern     = context.ast.patterns.push(wildcard_pattern(this_expression.range)),
                .initializer = context.desugar(discard.discarded_expression),
                .type        = context.ast.types.push(wildcard_type(this_expression.range)),
            };
            return ast::expression::Block {
                .side_effects = utl::to_vector({
                    ast::Expression { std::move(let), this_expression.range },
                }),
                .result       = context.ast.expressions.push(unit_value(this_expression.range)),
            };
        }

        auto operator()(cst::expression::Break const& break_expression) -> ast::Expression_variant
        {
            return ast::expression::Break {
                break_expression.result.has_value()
                    ? context.desugar(break_expression.result.value())
                    : context.ast.expressions.push(unit_value(this_expression.range)),
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
            auto const ampersand_range = context.cst.tokens[addressof.ampersand_token].range;
            return ast::expression::Addressof {
                .mutability = context.desugar_mutability(addressof.mutability, ampersand_range),
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
            return ast::expression::Unsafe { context.desugar(unsafe.expression) };
        }

        auto operator()(cst::expression::Move const& move) -> ast::Expression_variant
        {
            return ast::expression::Move { context.desugar(move.place_expression) };
        }

        auto operator()(cst::expression::Defer const& defer) -> ast::Expression_variant
        {
            return ast::expression::Defer { context.desugar(defer.effect_expression) };
        }

        auto operator()(cst::expression::Meta const& meta) -> ast::Expression_variant
        {
            return ast::expression::Meta { context.desugar(meta.expression.value) };
        }

        auto operator()(cst::expression::Hole const&) -> ast::Expression_variant
        {
            return ast::expression::Hole {};
        }

        auto operator()(cst::expression::For_loop const&) -> ast::Expression_variant
        {
            kieli::Diagnostic diagnostic {
                .message  = "For loops are not supported yet",
                .range    = this_expression.range,
                .severity = kieli::Severity::error,
            };
            kieli::add_diagnostic(context.db, context.document_id, std::move(diagnostic));
            return ast::expression::Error {};
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
    Expression_desugaring_visitor visitor { *this, expression };
    return { std::visit(visitor, expression.variant), expression.range };
}
