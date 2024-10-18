#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    auto extract_string_literal(Context& context, Token const& token) -> cst::Expression_variant
    {
        auto const first_string = token.value_as<kieli::String>();
        if (context.peek().type != Token_type::string_literal) {
            return first_string;
        }
        std::string combined_string { first_string.value.view() };
        while (auto const token = context.try_extract(Token_type::string_literal)) {
            combined_string.append(token.value().value_as<kieli::String>().value.view());
        }
        return kieli::String { context.db().string_pool.add(combined_string) };
    }

    auto extract_condition(Context& context) -> cst::Expression_id
    {
        if (auto const let_keyword = context.try_extract(Token_type::let)) {
            auto const pattern     = require<parse_pattern>(context, "a pattern");
            auto const equals_sign = context.require_extract(Token_type::equals);
            return context.cst().expressions.push(
                cst::expression::Conditional_let {
                    .pattern     = pattern,
                    .initializer = require<parse_expression>(context, "the initializer expression"),
                    .let_keyword_token = context.token(let_keyword.value()),
                    .equals_sign_token = context.token(equals_sign),
                },
                context.up_to_current(let_keyword.value().range));
        }
        return require<parse_expression>(context, "the condition expression");
    }

    auto extract_loop_body(Context& context) -> cst::Expression_id
    {
        return require<parse_block_expression>(
            context, "the loop body, which must be a block expression");
    }

    auto extract_plain_loop(Context& context, Token const& loop_keyword) -> cst::Expression_variant
    {
        return cst::expression::Plain_loop {
            .body               = extract_loop_body(context),
            .loop_keyword_token = context.token(loop_keyword),
        };
    }

    auto extract_while_loop(Context& context, Token const& while_keyword) -> cst::Expression_variant
    {
        return cst::expression::While_loop {
            .condition           = extract_condition(context),
            .body                = extract_loop_body(context),
            .while_keyword_token = context.token(while_keyword),
        };
    }

    auto extract_for_loop(Context& context, Token const& for_keyword) -> cst::Expression_variant
    {
        cst::Pattern_id const iterator_pattern = require<parse_pattern>(context, "a pattern");
        Token const           in_keyword       = context.require_extract(Token_type::in);
        return cst::expression::For_loop {
            .iterator          = iterator_pattern,
            .iterable          = require<parse_expression>(context, "an iterable expression"),
            .body              = extract_loop_body(context),
            .for_keyword_token = context.token(for_keyword),
            .in_keyword_token  = context.token(in_keyword),
        };
    }

    auto parse_struct_field_initializer(Context& context)
        -> std::optional<cst::Struct_field_initializer>
    {
        return parse_lower_name(context).transform([&](kieli::Lower const name) {
            return cst::Struct_field_initializer {
                name,
                context.try_extract(Token_type::equals).transform([&](Token const equals_sign) {
                    return cst::Struct_field_equals {
                        context.token(equals_sign),
                        require<parse_expression>(context, "an initializer expression"),
                    };
                }),
            };
        });
    }

    auto extract_dereference(Context& context, Token const& asterisk) -> cst::Expression_variant
    {
        return cst::expression::Dereference {
            .reference_expression = require<parse_expression>(context, "an expression"),
            .asterisk_token       = context.token(asterisk),
        };
    }

    auto extract_tuple(Context& context, Token const& paren_open) -> cst::Expression_variant
    {
        auto expressions
            = extract_comma_separated_zero_or_more<parse_expression, "an expression">(context);
        Token const paren_close = context.require_extract(Token_type::paren_close);
        if (expressions.elements.size() == 1) {
            return cst::expression::Parenthesized { {
                .value       = expressions.elements.front(),
                .open_token  = context.token(paren_open),
                .close_token = context.token(paren_close),
            } };
        }
        return cst::expression::Tuple { {
            .value       = std::move(expressions),
            .open_token  = context.token(paren_open),
            .close_token = context.token(paren_close),
        } };
    }

    auto extract_array(Context& context, Token const& bracket_open) -> cst::Expression_variant
    {
        auto elements
            = extract_comma_separated_zero_or_more<parse_expression, "an array element">(context);
        if (auto const bracket_close = context.try_extract(Token_type::bracket_close)) {
            return cst::expression::Array_literal { {
                .value       = std::move(elements),
                .open_token  = context.token(bracket_open),
                .close_token = context.token(bracket_close.value()),
            } };
        }
        if (elements.elements.empty()) {
            context.error_expected("an array element or a ']'");
        }
        else {
            context.error_expected("a ',' or a ']'");
        }
    }

    enum struct Conditional_kind { if_, elif };

    auto extract_conditional(
        Context&               context,
        Token const&           if_or_elif_keyword,
        Conditional_kind const kind = Conditional_kind::if_) -> cst::Expression_variant
    {
        auto const primary_condition = extract_condition(context);
        auto const true_branch = require<parse_block_expression>(context, "a block expression");

        std::optional<cst::expression::False_branch> false_branch;

        if (auto const elif_keyword = context.try_extract(Token_type::elif)) {
            auto elif_conditional
                = extract_conditional(context, elif_keyword.value(), Conditional_kind::elif);
            false_branch = cst::expression::False_branch {
                .body = context.cst().expressions.push(
                    std::move(elif_conditional), context.up_to_current(elif_keyword.value().range)),
                .else_or_elif_keyword_token = context.token(elif_keyword.value()),
            };
        }
        else if (auto const else_keyword = context.try_extract(Token_type::else_)) {
            false_branch = cst::expression::False_branch {
                .body = require<parse_block_expression>(context, "an else-block expression"),
                .else_or_elif_keyword_token = context.token(else_keyword.value()),
            };
        }

        return cst::expression::Conditional {
            .condition                = primary_condition,
            .true_branch              = true_branch,
            .false_branch             = false_branch,
            .if_or_elif_keyword_token = context.token(if_or_elif_keyword),
            .is_elif                  = kind == Conditional_kind::elif,
        };
    }

    auto extract_let_binding(Context& context, Token const& let_keyword) -> cst::Expression_variant
    {
        auto const pattern     = require<parse_top_level_pattern>(context, "a pattern");
        auto const type        = parse_type_annotation(context);
        auto const equals_sign = context.require_extract(Token_type::equals);
        return cst::expression::Let_binding {
            .pattern           = pattern,
            .type              = type,
            .initializer       = require<parse_expression>(context, "the initializer expression"),
            .let_keyword_token = context.token(let_keyword),
            .equals_sign_token = context.token(equals_sign),
        };
    }

    auto extract_local_type_alias(Context& context, Token const& alias_keyword)
        -> cst::Expression_variant
    {
        auto const name        = extract_upper_name(context, "an alias name");
        auto const equals_sign = context.require_extract(Token_type::equals);
        return cst::expression::Local_type_alias {
            .name                = name,
            .type                = require<parse_type>(context, "an aliased type"),
            .alias_keyword_token = context.token(alias_keyword),
            .equals_sign_token   = context.token(equals_sign),
        };
    }

    auto extract_sizeof(Context& context, Token const& sizeof_keyword) -> cst::Expression_variant
    {
        return cst::expression::Sizeof {
            .inspected_type
            = require<parse_parenthesized<parse_type, "a type">>(context, "a parenthesized type"),
            .sizeof_keyword_token = context.token(sizeof_keyword),
        };
    }

    auto parse_match_case(Context& context) -> std::optional<cst::Match_case>
    {
        return parse_top_level_pattern(context).transform([&](cst::Pattern_id const pattern) {
            auto const arrow     = context.require_extract(Token_type::right_arrow);
            auto const handler   = require<parse_expression>(context, "an expression");
            auto const semicolon = context.try_extract(Token_type::semicolon);
            return cst::Match_case {
                .pattern                  = pattern,
                .handler                  = handler,
                .arrow_token              = context.token(arrow),
                .optional_semicolon_token = semicolon.transform( //
                    std::bind_front(&Context::token, std::ref(context))),
            };
        });
    }

    auto parse_match_cases(Context& context) -> std::optional<std::vector<cst::Match_case>>
    {
        std::vector<cst::Match_case> cases;
        while (auto match_case = parse_match_case(context)) {
            cases.push_back(std::move(match_case.value()));
        }
        return not cases.empty() ? std::optional(std::move(cases)) : std::nullopt;
    }

    auto extract_match(Context& context, Token const& match_keyword) -> cst::Expression_variant
    {
        static constexpr auto extract_cases
            = require<parse_braced<parse_match_cases, "one or more match cases">>;
        auto const expression = require<parse_expression>(context, "an expression");
        return cst::expression::Match {
            .cases               = extract_cases(context, "a '{' followed by match cases"),
            .matched_expression  = expression,
            .match_keyword_token = context.token(match_keyword),
        };
    }

    auto extract_continue(Context& context, Token const& continue_keyword)
        -> cst::Expression_variant
    {
        return cst::expression::Continue { context.token(continue_keyword) };
    }

    auto extract_break(Context& context, Token const& break_keyword) -> cst::Expression_variant
    {
        return cst::expression::Break {
            .result              = parse_expression(context),
            .break_keyword_token = context.token(break_keyword),
        };
    }

    auto extract_ret(Context& context, Token const& ret_keyword) -> cst::Expression_variant
    {
        return cst::expression::Ret {
            .returned_expression = parse_expression(context),
            .ret_keyword_token   = context.token(ret_keyword),
        };
    }

    auto extract_discard(Context& context, Token const& discard_keyword) -> cst::Expression_variant
    {
        return cst::expression::Discard {
            .discarded_expression  = require<parse_expression>(context, "the discarded expression"),
            .discard_keyword_token = context.token(discard_keyword),
        };
    }

    auto extract_addressof(Context& context, Token const& ampersand) -> cst::Expression_variant
    {
        return cst::expression::Addressof {
            .mutability       = parse_mutability(context),
            .place_expression = require<parse_expression>(context, "the referenced expression"),
            .ampersand_token  = context.token(ampersand),
        };
    }

    auto extract_move(Context& context, Token const& mov_keyword) -> cst::Expression_variant
    {
        return cst::expression::Move {
            .place_expression  = require<parse_expression>(context, "a place expression"),
            .mov_keyword_token = context.token(mov_keyword),
        };
    }

    auto extract_defer(Context& context, Token const& defer_keyword) -> cst::Expression_variant
    {
        return cst::expression::Defer {
            .effect_expression   = require<parse_expression>(context, "an expression"),
            .defer_keyword_token = context.token(defer_keyword),
        };
    }

    auto extract_block_expression(Context& context, Token const& brace_open)
        -> cst::Expression_variant
    {
        std::vector<cst::expression::Block::Side_effect> side_effects;
        std::optional<cst::Expression_id>                result_expression;

        while (auto expression = parse_expression(context)) {
            if (auto const semicolon = context.try_extract(Token_type::semicolon)) {
                side_effects.push_back(cst::expression::Block::Side_effect {
                    .expression               = expression.value(),
                    .trailing_semicolon_token = context.token(semicolon.value()),
                });
            }
            else {
                result_expression = expression.value();
                break;
            }
        }

        Token const brace_close = context.require_extract(Token_type::brace_close);
        return cst::expression::Block {
            .side_effects      = std::move(side_effects),
            .result_expression = std::move(result_expression),
            .open_brace_token  = context.token(brace_open),
            .close_brace_token = context.token(brace_close),
        };
    }

    auto try_extract_initializer(Context& context, cst::Path&& path) -> cst::Expression_variant
    {
        static constexpr auto parse_braced_initializers = parse_braced<
            parse_comma_separated_one_or_more<
                parse_struct_field_initializer,
                "a field initializer">,
            "one or more field initializers">;
        static constexpr auto parse_parenthesized_initializers = parse_parenthesized<
            parse_comma_separated_one_or_more<parse_expression, "an expression">,
            "one or more expressions">;

        if (path.is_upper()) {
            if (auto initializers = parse_braced_initializers(context)) {
                return cst::expression::Struct_initializer {
                    .constructor_path = std::move(path),
                    .initializers     = std::move(initializers.value()),
                };
            }
            if (auto initializers = parse_parenthesized_initializers(context)) {
                return cst::expression::Tuple_initializer {
                    .constructor_path = std::move(path),
                    .initializers     = std::move(initializers.value()),
                };
            }
        }
        return path;
    }

    auto dispatch_parse_normal_expression(Context& context)
        -> std::optional<cst::Expression_variant>
    {
        switch (context.peek().type) {
        case Token_type::underscore:        return cst::Wildcard { context.token(context.extract()) };
        case Token_type::integer_literal:   return context.extract().value_as<kieli::Integer>();
        case Token_type::floating_literal:  return context.extract().value_as<kieli::Floating>();
        case Token_type::character_literal: return context.extract().value_as<kieli::Character>();
        case Token_type::boolean_literal:   return context.extract().value_as<kieli::Boolean>();
        case Token_type::string_literal:    return extract_string_literal(context, context.extract());
        case Token_type::asterisk:          return extract_dereference(context, context.extract());
        case Token_type::paren_open:        return extract_tuple(context, context.extract());
        case Token_type::bracket_open:      return extract_array(context, context.extract());
        case Token_type::if_:               return extract_conditional(context, context.extract());
        case Token_type::let:               return extract_let_binding(context, context.extract());
        case Token_type::alias:             return extract_local_type_alias(context, context.extract());
        case Token_type::loop:              return extract_plain_loop(context, context.extract());
        case Token_type::while_:            return extract_while_loop(context, context.extract());
        case Token_type::for_:              return extract_for_loop(context, context.extract());
        case Token_type::sizeof_:           return extract_sizeof(context, context.extract());
        case Token_type::match:             return extract_match(context, context.extract());
        case Token_type::continue_:         return extract_continue(context, context.extract());
        case Token_type::break_:            return extract_break(context, context.extract());
        case Token_type::ret:               return extract_ret(context, context.extract());
        case Token_type::discard:           return extract_discard(context, context.extract());
        case Token_type::ampersand:         return extract_addressof(context, context.extract());
        case Token_type::mv:                return extract_move(context, context.extract());
        case Token_type::defer:             return extract_defer(context, context.extract());
        case Token_type::brace_open:        return extract_block_expression(context, context.extract());
        default:
            return parse_complex_path(context).transform([&](cst::Path&& path) {
                return try_extract_initializer(context, std::move(path));
            });
        }
    }

    auto parse_normal_expression(Context& context) -> std::optional<cst::Expression_id>
    {
        kieli::Range const anchor_range = context.peek().range;
        return dispatch_parse_normal_expression(context).transform(
            [&](cst::Expression_variant&& variant) {
                return context.cst().expressions.push(
                    std::move(variant), context.up_to_current(anchor_range));
            });
    }

    auto parse_potential_function_call(Context& context) -> std::optional<cst::Expression_id>
    {
        return parse_normal_expression(context).transform([&](cst::Expression_id expression) {
            while (auto arguments = parse_function_arguments(context)) {
                expression = context.cst().expressions.push(
                    cst::expression::Function_call {
                        .arguments = std::move(arguments.value()),
                        .invocable = expression,
                    },
                    context.up_to_current(context.cst().expressions[expression].range));
            }
            return expression;
        });
    }

    auto extract_struct_field_access(
        kieli::Lower const       field_name,
        cst::Token_id const      dot_token,
        cst::Expression_id const expression,
        Context&                 context) -> cst::Expression_variant
    {
        auto template_arguments = parse_template_arguments(context);
        if (auto arguments = parse_function_arguments(context)) {
            return cst::expression::Method_call {
                .function_arguments = std::move(arguments.value()),
                .template_arguments = std::move(template_arguments),
                .base_expression    = expression,
                .method_name        = field_name,
            };
        }
        if (template_arguments.has_value()) {
            context.error_expected("a parenthesized argument set");
        }
        return cst::expression::Struct_field_access {
            .base_expression = expression,
            .field_name      = field_name,
            .dot_token       = dot_token,
        };
    }

    auto extract_member_access(
        cst::Token_id const      dot_token,
        cst::Expression_id const base_expression,
        Context&                 context) -> cst::Expression_variant
    {
        if (auto const name = parse_lower_name(context)) {
            return extract_struct_field_access(name.value(), dot_token, base_expression, context);
        }
        if (auto const index = parse_bracketed<parse_expression, "an index expression">(context)) {
            return cst::expression::Array_index_access {
                .base_expression  = base_expression,
                .index_expression = index.value(),
                .dot_token        = dot_token,
            };
        }
        if (auto const literal = context.try_extract(Token_type::integer_literal)) {
            return cst::expression::Tuple_field_access {
                .base_expression   = base_expression,
                .field_index       = literal.value().value_as<kieli::Integer>().value,
                .field_index_token = context.token(literal.value()),
                .dot_token         = dot_token,
            };
        }
        context.error_expected(
            "a struct member name (a.b), a tuple member index (a.0), or an array index (a.[b])");
    }

    auto parse_potential_member_access(Context& context) -> std::optional<cst::Expression_id>
    {
        return parse_potential_function_call(context).transform([&](cst::Expression_id expression) {
            while (auto const dot = context.try_extract(Token_type::dot)) {
                expression = context.cst().expressions.push(
                    extract_member_access(context.token(dot.value()), expression, context),
                    context.up_to_current(context.cst().expressions[expression].range));
            }
            return expression;
        });
    }

    auto dispatch_parse_type_cast(Context& context, cst::Expression_id const base_expression)
        -> std::optional<cst::Expression_variant>
    {
        switch (context.peek().type) {
        case Token_type::colon:
            return cst::expression::Type_ascription {
                .base_expression = base_expression,
                .colon_token     = context.token(context.extract()),
                .ascribed_type   = require<parse_type>(context, "the ascribed type"),
            };
        case Token_type::as:
            return cst::expression::Type_cast {
                .base_expression = base_expression,
                .as_token        = context.token(context.extract()),
                .target_type     = require<parse_type>(context, "the target type"),
            };
        default: return std::nullopt;
        }
    }

    auto parse_potential_type_cast(Context& context) -> std::optional<cst::Expression_id>
    {
        return parse_potential_member_access(context).transform([&](cst::Expression_id expression) {
            while (auto type_cast = dispatch_parse_type_cast(context, expression)) {
                expression = context.cst().expressions.push(
                    std::move(type_cast.value()),
                    context.up_to_current(context.cst().expressions[expression].range));
            }
            return expression;
        });
    }

    auto parse_operator_id(Context& context) -> std::optional<kieli::Identifier>
    {
        switch (context.peek().type) {
        case Token_type::operator_name:
        {
            return context.extract().value_as<kieli::Identifier>();
        }
        case Token_type::asterisk:
        {
            (void)context.extract();
            return context.special_identifiers().asterisk;
        }
        case Token_type::plus:
        {
            (void)context.extract();
            return context.special_identifiers().plus;
        }
        default: return std::nullopt;
        }
    }

    auto parse_infix_name(Context& context) -> std::optional<cst::expression::Infix_name>
    {
        kieli::Range const anchor_range = context.peek().range;
        return parse_operator_id(context).transform([&](kieli::Identifier const identifier) {
            return cst::expression::Infix_name { identifier, anchor_range };
        });
    }

    auto parse_infix_chain(Context& context) -> std::optional<cst::Expression_id>
    {
        return parse_potential_type_cast(context).transform(
            [&](cst::Expression_id const expression) {
                std::vector<cst::expression::Infix_chain::Rhs> tail;
                while (auto const operator_name = parse_infix_name(context)) {
                    tail.push_back({
                        .operand    = require<parse_potential_type_cast>(context, "an operand"),
                        .infix_name = operator_name.value(),
                    });
                }
                if (tail.empty()) {
                    return expression;
                }
                return context.cst().expressions.push(
                    cst::expression::Infix_chain { .tail = std::move(tail), .lhs = expression },
                    context.up_to_current(context.cst().expressions[expression].range));
            });
    }
} // namespace

auto libparse::parse_block_expression(Context& context) -> std::optional<cst::Expression_id>
{
    bool const is_block = context.peek().type == Token_type::brace_open;
    return is_block ? parse_normal_expression(context) : std::nullopt;
}

auto libparse::parse_expression(Context& context) -> std::optional<cst::Expression_id>
{
    return parse_infix_chain(context);
}
