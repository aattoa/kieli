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

    auto extract_condition(Context& context) -> utl::Wrapper<cst::Expression>
    {
        if (auto const let_keyword = context.try_extract(Token_type::let)) {
            auto const pattern     = require<parse_pattern>(context, "a pattern");
            auto const equals_sign = context.require_extract(Token_type::equals);
            return context.wrap(cst::Expression {
                .variant { cst::expression::Conditional_let {
                    .pattern     = pattern,
                    .initializer = require<parse_expression>(context, "the initializer expression"),
                    .let_keyword_token = cst::Token::from_lexical(let_keyword.value()),
                    .equals_sign_token = cst::Token::from_lexical(equals_sign),
                } },
                .range = context.up_to_current(let_keyword.value().range),
            });
        }
        return require<parse_expression>(context, "the condition expression");
    }

    auto extract_loop_body(Context& context) -> utl::Wrapper<cst::Expression>
    {
        return require<parse_block_expression>(
            context, "the loop body, which must be a block expression");
    }

    auto extract_infinite_loop(Context& context, Token const& loop_keyword)
        -> cst::Expression_variant
    {
        return cst::expression::Infinite_loop {
            .body               = extract_loop_body(context),
            .loop_keyword_token = cst::Token::from_lexical(loop_keyword),
        };
    }

    auto extract_while_loop(Context& context, Token const& while_keyword) -> cst::Expression_variant
    {
        return cst::expression::While_loop {
            .condition           = extract_condition(context),
            .body                = extract_loop_body(context),
            .while_keyword_token = cst::Token::from_lexical(while_keyword),
        };
    }

    auto extract_for_loop(Context& context, Token const& for_keyword) -> cst::Expression_variant
    {
        auto const  iterator   = require<parse_pattern>(context, "a pattern");
        Token const in_keyword = context.require_extract(Token_type::in);
        return cst::expression::For_loop {
            .iterator          = iterator,
            .iterable          = require<parse_expression>(context, "an iterable expression"),
            .body              = extract_loop_body(context),
            .for_keyword_token = cst::Token::from_lexical(for_keyword),
            .in_keyword_token  = cst::Token::from_lexical(in_keyword),
        };
    }

    auto parse_struct_field_initializer(Context& context)
        -> std::optional<cst::expression::Struct_initializer::Field>
    {
        return parse_lower_name(context).transform([&](kieli::Lower const name) {
            Token const equals_sign = context.require_extract(Token_type::equals);
            return cst::expression::Struct_initializer::Field {
                .name              = name,
                .equals_sign_token = cst::Token::from_lexical(equals_sign),
                .expression = require<parse_expression>(context, "an initializer expression"),
            };
        });
    }

    auto extract_expression_path(Context& context, std::optional<cst::Path_root>&& root)
        -> cst::Expression_variant
    {
        auto path = extract_path(context, std::move(root));
        if (!path.head.is_upper()) {
            auto template_arguments = parse_template_arguments(context);
            if (template_arguments) {
                return cst::expression::Template_application {
                    .template_arguments = std::move(template_arguments.value()),
                    .path               = std::move(path),
                };
            }
            return cst::expression::Variable { std::move(path) };
        }

        static constexpr parser auto parse_braced_initializers = parse_braced<
            parse_comma_separated_one_or_more<
                parse_struct_field_initializer,
                "a field initializer">,
            "one or more field initializers">;
        static constexpr parser auto parse_parenthesized_initializers = parse_parenthesized<
            parse_comma_separated_one_or_more<parse_expression, "an expression">,
            "one or more expressions">;

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
        return cst::expression::Unit_initializer { .constructor_path = std::move(path) };
    }

    auto extract_identifier(Context& context, Stage const stage) -> cst::Expression_variant
    {
        context.unstage(stage);
        return extract_expression_path(context, std::nullopt);
    }

    auto extract_global_identifier(Context& context, Token const& global) -> cst::Expression_variant
    {
        return extract_expression_path(
            context,
            cst::Path_root {
                .variant = cst::Path_root_global {
                    .global_keyword = cst::Token::from_lexical(global),
                },
                .double_colon_token
                    = cst::Token::from_lexical(context.require_extract(Token_type::double_colon)),
                .range = global.range,
            });
    }

    auto extract_dereference(Context& context, Token const& asterisk) -> cst::Expression_variant
    {
        return cst::expression::Dereference {
            .reference_expression = require<parse_expression>(context, "an expression"),
            .asterisk_token       = cst::Token::from_lexical(asterisk),
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
                .open_token  = cst::Token::from_lexical(paren_open),
                .close_token = cst::Token::from_lexical(paren_close),
            } };
        }
        return cst::expression::Tuple { {
            .value       = std::move(expressions),
            .open_token  = cst::Token::from_lexical(paren_open),
            .close_token = cst::Token::from_lexical(paren_close),
        } };
    }

    auto extract_array(Context& context, Token const& bracket_open) -> cst::Expression_variant
    {
        auto elements
            = extract_comma_separated_zero_or_more<parse_expression, "an array element">(context);
        if (auto const bracket_close = context.try_extract(Token_type::bracket_close)) {
            return cst::expression::Array_literal { {
                .value       = std::move(elements),
                .open_token  = cst::Token::from_lexical(bracket_open),
                .close_token = cst::Token::from_lexical(bracket_close.value()),
            } };
        }
        if (elements.elements.empty()) {
            context.error_expected("an array element or a ']'");
        }
        else {
            context.error_expected("a ',' or a ']'");
        }
    }

    enum class Conditional_kind { if_, elif };

    auto extract_conditional(
        Context&               context,
        Token const&           if_or_elif_keyword,
        Conditional_kind const conditional_kind) -> cst::Expression_variant
    {
        static constexpr auto extract_branch = [](Context& context) {
            if (auto const branch = parse_block_expression(context)) {
                return branch.value();
            }
            context.error_expected("a block expression");
        };

        // TODO: cleanup

        auto const primary_condition = extract_condition(context);
        auto const true_branch       = extract_branch(context);

        std::optional<cst::expression::Conditional::False_branch> false_branch;

        if (auto const elif_keyword = context.try_extract(Token_type::elif)) {
            auto elif_conditional
                = extract_conditional(context, elif_keyword.value(), Conditional_kind::elif);

            false_branch = cst::expression::Conditional::False_branch {
                .body = context.wrap(cst::Expression {
                    .variant = std::move(elif_conditional),
                    .range   = context.up_to_current(elif_keyword.value().range),
                }),

                .else_or_elif_keyword_token = cst::Token::from_lexical(elif_keyword.value()),
            };
        }
        else if (auto const else_keyword = context.try_extract(Token_type::else_)) {
            false_branch = cst::expression::Conditional::False_branch {
                .body                       = extract_branch(context),
                .else_or_elif_keyword_token = cst::Token::from_lexical(else_keyword.value()),
            };
        }

        return cst::expression::Conditional {
            .condition                = primary_condition,
            .true_branch              = true_branch,
            .false_branch             = false_branch,
            .if_or_elif_keyword_token = cst::Token::from_lexical(if_or_elif_keyword),
            .is_elif                  = conditional_kind == Conditional_kind::elif,
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
            .let_keyword_token = cst::Token::from_lexical(let_keyword),
            .equals_sign_token = cst::Token::from_lexical(equals_sign),
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
            .alias_keyword_token = cst::Token::from_lexical(alias_keyword),
            .equals_sign_token   = cst::Token::from_lexical(equals_sign),
        };
    }

    auto extract_sizeof(Context& context, Token const& sizeof_keyword) -> cst::Expression_variant
    {
        return cst::expression::Sizeof {
            .inspected_type
            = require<parse_parenthesized<parse_type, "a type">>(context, "a parenthesized type"),
            .sizeof_keyword_token = cst::Token::from_lexical(sizeof_keyword),
        };
    }

    auto parse_match_case(Context& context) -> std::optional<cst::expression::Match::Case>
    {
        return parse_top_level_pattern(context).transform(
            [&](utl::Wrapper<cst::Pattern> const pattern) {
                auto const arrow     = context.require_extract(Token_type::right_arrow);
                auto const handler   = require<parse_expression>(context, "an expression");
                auto const semicolon = context.try_extract(Token_type::semicolon);
                return cst::expression::Match::Case {
                    .pattern                  = pattern,
                    .handler                  = handler,
                    .arrow_token              = cst::Token::from_lexical(arrow),
                    .optional_semicolon_token = semicolon.transform(cst::Token::from_lexical),
                };
            });
    }

    auto parse_match_cases(Context& context)
        -> std::optional<std::vector<cst::expression::Match::Case>>
    {
        std::vector<cst::expression::Match::Case> cases;
        while (auto match_case = parse_match_case(context)) {
            cases.push_back(std::move(match_case.value()));
        }
        return !cases.empty() ? std::optional(std::move(cases)) : std::nullopt;
    }

    auto extract_match(Context& context, Token const& match_keyword) -> cst::Expression_variant
    {
        static constexpr auto extract_cases
            = require<parse_braced<parse_match_cases, "one or more match cases">>;
        auto const expression = require<parse_expression>(context, "an expression");
        return cst::expression::Match {
            .cases               = extract_cases(context, "a '{' followed by match cases"),
            .matched_expression  = expression,
            .match_keyword_token = cst::Token::from_lexical(match_keyword),
        };
    }

    auto extract_continue(Context&, Token const& continue_keyword) -> cst::Expression_variant
    {
        return cst::expression::Continue {
            .continue_keyword_token = cst::Token::from_lexical(continue_keyword),
        };
    }

    auto extract_break(Context& context, Token const& break_keyword) -> cst::Expression_variant
    {
        return cst::expression::Break {
            .result              = parse_expression(context),
            .break_keyword_token = cst::Token::from_lexical(break_keyword),
        };
    }

    auto extract_ret(Context& context, Token const& ret_keyword) -> cst::Expression_variant
    {
        return cst::expression::Ret {
            .returned_expression = parse_expression(context),
            .ret_keyword_token   = cst::Token::from_lexical(ret_keyword),
        };
    }

    auto extract_discard(Context& context, Token const& discard_keyword) -> cst::Expression_variant
    {
        return cst::expression::Discard {
            .discarded_expression  = require<parse_expression>(context, "the discarded expression"),
            .discard_keyword_token = cst::Token::from_lexical(discard_keyword),
        };
    }

    auto extract_addressof(Context& context, Token const& ampersand) -> cst::Expression_variant
    {
        return cst::expression::Addressof {
            .mutability       = parse_mutability(context),
            .place_expression = require<parse_expression>(context, "the referenced expression"),
            .ampersand_token  = cst::Token::from_lexical(ampersand),
        };
    }

    auto extract_unsafe_block(Context& context, Token const& unsafe_keyword)
        -> cst::Expression_variant
    {
        return cst::expression::Unsafe {
            .expression = require<parse_block_expression>(context, "an unsafe block expression"),
            .unsafe_keyword_token = cst::Token::from_lexical(unsafe_keyword),
        };
    }

    auto extract_move(Context& context, Token const& mov_keyword) -> cst::Expression_variant
    {
        return cst::expression::Move {
            .place_expression  = require<parse_expression>(context, "a place expression"),
            .mov_keyword_token = cst::Token::from_lexical(mov_keyword),
        };
    }

    auto extract_defer(Context& context, Token const& defer_keyword) -> cst::Expression_variant
    {
        return cst::expression::Defer {
            .expression          = require<parse_expression>(context, "an expression"),
            .defer_keyword_token = cst::Token::from_lexical(defer_keyword),
        };
    }

    auto extract_meta(Context& context, Token const& meta_keyword) -> cst::Expression_variant
    {
        return cst::expression::Meta {
            .expression = require<parse_parenthesized<parse_expression, "an expression">>(
                context, "a parenthesized expression"),
            .meta_keyword_token = cst::Token::from_lexical(meta_keyword),
        };
    }

    auto extract_block_expression(Context& context, Token const& brace_open)
        -> cst::Expression_variant
    {
        std::vector<cst::expression::Block::Side_effect> side_effects;
        std::optional<utl::Wrapper<cst::Expression>>     result_expression;

        while (auto expression = parse_expression(context)) {
            if (auto const semicolon = context.try_extract(Token_type::semicolon)) {
                side_effects.push_back(cst::expression::Block::Side_effect {
                    .expression               = expression.value(),
                    .trailing_semicolon_token = cst::Token::from_lexical(semicolon.value()),
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
            .open_brace_token  = cst::Token::from_lexical(brace_open),
            .close_brace_token = cst::Token::from_lexical(brace_close),
        };
    }

    auto parse_complicated_type(Context& context, Stage const stage)
        -> std::optional<cst::Expression_variant>
    {
        context.unstage(stage);
        return parse_type(context).transform(
            [&](utl::Wrapper<cst::Type> const type) -> cst::Expression_variant {
                if (auto const double_colon = context.try_extract(Token_type::double_colon)) {
                    return extract_expression_path(
                        context,
                        cst::Path_root {
                            .variant            = type,
                            .double_colon_token = cst::Token::from_lexical(double_colon.value()),
                            .range              = type->range,
                        });
                }
                kieli::fatal_error(
                    context.db(),
                    context.source(),
                    type->range,
                    "Expected an expression, but found a type");
            });
    }

    auto dispatch_parse_normal_expression(Context& context, Token const& token, Stage const stage)
        -> std::optional<cst::Expression_variant>
    {
        // clang-format off
        switch (token.type) {
        case Token_type::integer_literal:   return token.value_as<kieli::Integer>();
        case Token_type::floating_literal:  return token.value_as<kieli::Floating>();
        case Token_type::character_literal: return token.value_as<kieli::Character>();
        case Token_type::boolean_literal:   return token.value_as<kieli::Boolean>();
        case Token_type::string_literal:    return extract_string_literal(context, token);
        case Token_type::lower_name:        [[fallthrough]];
        case Token_type::upper_name:        return extract_identifier(context, stage);
        case Token_type::global:            return extract_global_identifier(context, token);
        case Token_type::lower_self:        return cst::expression::Self {};
        case Token_type::hole:              return cst::expression::Hole {};
        case Token_type::asterisk:          return extract_dereference(context, token);
        case Token_type::paren_open:        return extract_tuple(context, token);
        case Token_type::bracket_open:      return extract_array(context, token);
        case Token_type::if_:               return extract_conditional(context, token, Conditional_kind::if_);
        case Token_type::let:               return extract_let_binding(context, token);
        case Token_type::alias:             return extract_local_type_alias(context, token);
        case Token_type::loop:              return extract_infinite_loop(context, token);
        case Token_type::while_:            return extract_while_loop(context, token);
        case Token_type::for_:              return extract_for_loop(context, token);
        case Token_type::sizeof_:           return extract_sizeof(context, token);
        case Token_type::unsafe:            return extract_unsafe_block(context, token);
        case Token_type::match:             return extract_match(context, token);
        case Token_type::continue_:         return extract_continue(context, token);
        case Token_type::break_:            return extract_break(context, token);
        case Token_type::ret:               return extract_ret(context, token);
        case Token_type::discard:           return extract_discard(context, token);
        case Token_type::ampersand:         return extract_addressof(context, token);
        case Token_type::mov:               return extract_move(context, token);
        case Token_type::defer:             return extract_defer(context, token);
        case Token_type::meta:              return extract_meta(context, token);
        case Token_type::brace_open:        return extract_block_expression(context, token);
        default:                             return parse_complicated_type(context, stage);
            // clang-format on
        }
    }

    auto parse_normal_expression(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
    {
        Stage const stage       = context.stage();
        Token const first_token = context.extract();
        if (auto variant = dispatch_parse_normal_expression(context, first_token, stage)) {
            return context.wrap(cst::Expression {
                .variant = std::move(variant.value()),
                .range   = context.up_to_current(first_token.range),
            });
        }
        context.unstage(stage);
        return std::nullopt;
    }

    auto parse_potential_invocation(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_normal_expression(context).transform(
            [&](utl::Wrapper<cst::Expression> expression) {
                while (auto arguments = parse_function_arguments(context)) {
                    expression = context.wrap(cst::Expression {
                        cst::expression::Invocation {
                            .function_arguments  = std::move(arguments.value()),
                            .function_expression = expression,
                        },
                        context.up_to_current(expression->range),
                    });
                }
                return expression;
            });
    }

    auto extract_struct_field_access(
        kieli::Lower const                  field_name,
        cst::Token const                    dot_token,
        utl::Wrapper<cst::Expression> const expression,
        Context&                            context) -> cst::Expression_variant
    {
        auto template_arguments = parse_template_arguments(context);
        if (auto arguments = parse_function_arguments(context)) {
            return cst::expression::Method_invocation {
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
        cst::Token const                    dot_token,
        utl::Wrapper<cst::Expression> const base_expression,
        Context&                            context) -> cst::Expression_variant
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
                .field_index_token = cst::Token::from_lexical(literal.value()),
                .dot_token         = dot_token,
            };
        }
        context.error_expected(
            "a struct member name (a.b), a tuple member index (a.0), or an array index (a.[b])");
    }

    auto parse_potential_member_access(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_potential_invocation(context).transform(
            [&](utl::Wrapper<cst::Expression> expression) {
                while (auto const dot = context.try_extract(Token_type::dot)) {
                    expression = context.wrap(cst::Expression {
                        extract_member_access(
                            cst::Token::from_lexical(dot.value()), expression, context),
                        context.up_to_current(expression->range),
                    });
                }
                return expression;
            });
    }

    auto dispatch_parse_type_cast(
        Context& context, utl::Wrapper<cst::Expression> const base_expression)
        -> std::optional<cst::Expression_variant>
    {
        switch (context.peek().type) {
        case Token_type::colon:
            return cst::expression::Type_ascription {
                .base_expression = base_expression,
                .colon_token     = cst::Token::from_lexical(context.extract()),
                .ascribed_type   = require<parse_type>(context, "the ascribed type"),
            };
        case Token_type::as:
            return cst::expression::Type_cast {
                .base_expression = base_expression,
                .as_token        = cst::Token::from_lexical(context.extract()),
                .target_type     = require<parse_type>(context, "the target type"),
            };
        default:
            return std::nullopt;
        }
    }

    auto parse_potential_type_cast(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_potential_member_access(context).transform(
            [&](utl::Wrapper<cst::Expression> expression) {
                while (auto type_cast = dispatch_parse_type_cast(context, expression)) {
                    expression = context.wrap(cst::Expression {
                        .variant = std::move(type_cast.value()),
                        .range   = context.up_to_current(expression->range),
                    });
                }
                return expression;
            });
    }

    auto parse_operator_id(Context& context) -> std::optional<kieli::Identifier>
    {
        Stage const stage = context.stage();
        Token const token = context.extract();
        switch (token.type) {
        case Token_type::operator_name:
            return token.value_as<kieli::Identifier>();
        case Token_type::asterisk:
            return context.special_identifiers().asterisk;
        case Token_type::plus:
            return context.special_identifiers().plus;
        default:
            context.unstage(stage);
            return std::nullopt;
        }
    }

    auto parse_operator_name(Context& context) -> std::optional<cst::expression::Operator_name>
    {
        auto const anchor_range = context.peek().range;
        return parse_operator_id(context).transform([&](kieli::Identifier const identifier) {
            return cst::expression::Operator_name {
                .identifier = identifier,
                .range      = anchor_range,
            };
        });
    }

    auto parse_binary_operator_chain(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_potential_type_cast(context).transform(
            [&](utl::Wrapper<cst::Expression> const expression) {
                std::vector<cst::expression::Operator_chain::Rhs> tail;
                while (auto const operator_name = parse_operator_name(context)) {
                    tail.push_back({
                        .operand       = require<parse_potential_type_cast>(context, "an operand"),
                        .operator_name = operator_name.value(),
                    });
                }
                if (tail.empty()) {
                    return expression;
                }
                return context.wrap(cst::Expression {
                    .variant = cst::expression::Operator_chain {
                        .tail = std::move(tail),
                        .lhs  = expression,
                    },
                    .range = context.up_to_current(expression->range),
                });
            });
    }
} // namespace

auto libparse::parse_block_expression(Context& context)
    -> std::optional<utl::Wrapper<cst::Expression>>
{
    return context.peek().type == Token_type::brace_open ? parse_normal_expression(context)
                                                         : std::nullopt;
}

auto libparse::parse_expression(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
{
    return parse_binary_operator_chain(context);
}
