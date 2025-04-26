#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>

using namespace ki;
using namespace ki::par;

namespace {
    auto extract_loop_body(Context& ctx) -> cst::Expression_id
    {
        return require<parse_block_expression>(
            ctx, "the loop body, which must be a block expression");
    }

    auto extract_plain_loop(Context& ctx, lex::Token const& loop_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, loop_keyword.range);
        return cst::expr::Loop {
            .body       = extract_loop_body(ctx),
            .loop_token = token(ctx, loop_keyword),
        };
    }

    auto extract_while_loop(Context& ctx, lex::Token const& while_keyword)
        -> cst::Expression_variant
    {
        add_keyword(ctx, while_keyword.range);
        return cst::expr::While_loop {
            .condition   = require<parse_expression>(ctx, "a condition"),
            .body        = extract_loop_body(ctx),
            .while_token = token(ctx, while_keyword),
        };
    }

    auto extract_for_loop(Context& ctx, lex::Token const& for_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, for_keyword.range);
        auto iterator   = require<parse_pattern>(ctx, "a pattern");
        auto in_keyword = require_extract(ctx, lex::Type::In);
        add_keyword(ctx, in_keyword.range);
        return cst::expr::For_loop {
            .iterator  = iterator,
            .iterable  = require<parse_expression>(ctx, "an iterable expression"),
            .body      = extract_loop_body(ctx),
            .for_token = token(ctx, for_keyword),
            .in_token  = token(ctx, in_keyword),
        };
    }

    auto parse_struct_field_initializer(Context& ctx) -> std::optional<cst::Struct_field_init>
    {
        return parse_lower_name(ctx).transform([&](db::Lower const name) {
            add_semantic_token(ctx, name.range, Semantic::Property);
            return cst::Struct_field_init {
                .name = name,
                .equals
                = try_extract(ctx, lex::Type::Equals).transform([&](lex::Token const equals_sign) {
                      return cst::Struct_field_equals {
                          .equals_sign_token = token(ctx, equals_sign),
                          .expression = require<parse_expression>(ctx, "an initializer expression"),
                      };
                  }),
            };
        });
    }

    auto extract_dereference(Context& ctx, lex::Token const& asterisk) -> cst::Expression_variant
    {
        return cst::expr::Deref {
            .expression     = require<parse_expression>(ctx, "an expression"),
            .asterisk_token = token(ctx, asterisk),
        };
    }

    auto extract_tuple(Context& ctx, lex::Token const& paren_open) -> cst::Expression_variant
    {
        add_punctuation(ctx, paren_open.range);
        auto fields = extract_comma_separated_zero_or_more<parse_expression, "an expression">(ctx);
        auto paren_close = require_extract(ctx, lex::Type::Paren_close);
        add_punctuation(ctx, paren_close.range);
        if (fields.elements.size() == 1) {
            return cst::expr::Paren { {
                .value       = fields.elements.front(),
                .open_token  = token(ctx, paren_open),
                .close_token = token(ctx, paren_close),
            } };
        }
        return cst::expr::Tuple { {
            .value       = std::move(fields),
            .open_token  = token(ctx, paren_open),
            .close_token = token(ctx, paren_close),
        } };
    }

    auto extract_array(Context& ctx, lex::Token const& bracket_open) -> cst::Expression_variant
    {
        auto elements
            = extract_comma_separated_zero_or_more<parse_expression, "an array element">(ctx);
        if (auto const bracket_close = try_extract(ctx, lex::Type::Bracket_close)) {
            return cst::expr::Array { {
                .value       = std::move(elements),
                .open_token  = token(ctx, bracket_open),
                .close_token = token(ctx, bracket_close.value()),
            } };
        }
        if (elements.elements.empty()) {
            error_expected(ctx, "an array element or a ']'");
        }
        else {
            error_expected(ctx, "a ',' or a ']'");
        }
    }

    auto extract_conditional(Context& ctx, lex::Token const& if_or_elif_keyword)
        -> cst::Expression_variant
    {
        add_keyword(ctx, if_or_elif_keyword.range);
        auto const condition   = require<parse_expression>(ctx, "a condition");
        auto const true_branch = require<parse_block_expression>(ctx, "a block expression");

        std::optional<cst::expr::False_branch> false_branch;

        if (auto const elif_keyword = try_extract(ctx, lex::Type::Elif)) {
            add_keyword(ctx, elif_keyword.value().range);
            auto elif_conditional = extract_conditional(ctx, elif_keyword.value());

            false_branch = cst::expr::False_branch {
                .body = ctx.arena.expressions.push(
                    std::move(elif_conditional), up_to_current(ctx, elif_keyword.value().range)),
                .keyword_token = token(ctx, elif_keyword.value()),
            };
        }
        else if (auto const else_keyword = try_extract(ctx, lex::Type::Else)) {
            add_keyword(ctx, else_keyword.value().range);
            false_branch = cst::expr::False_branch {
                .body          = require<parse_block_expression>(ctx, "an else-block expression"),
                .keyword_token = token(ctx, else_keyword.value()),
            };
        }

        return cst::expr::Conditional {
            .condition     = condition,
            .true_branch   = true_branch,
            .false_branch  = false_branch,
            .keyword_token = token(ctx, if_or_elif_keyword),
            .is_elif       = if_or_elif_keyword.type == lex::Type::Elif,
        };
    }

    auto extract_let(Context& ctx, lex::Token const& let_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, let_keyword.range);
        auto const pattern     = require<parse_top_level_pattern>(ctx, "a pattern");
        auto const type        = parse_type_annotation(ctx);
        auto const equals_sign = require_extract(ctx, lex::Type::Equals);
        add_semantic_token(ctx, equals_sign.range, Semantic::Operator_name);
        return cst::expr::Let {
            .pattern           = pattern,
            .type              = type,
            .initializer       = require<parse_expression>(ctx, "the initializer expression"),
            .let_token         = token(ctx, let_keyword),
            .equals_sign_token = token(ctx, equals_sign),
        };
    }

    auto extract_local_type_alias(Context& ctx, lex::Token const& alias_keyword)
        -> cst::Expression_variant
    {
        add_keyword(ctx, alias_keyword.range);
        auto const name = extract_upper_name(ctx, "an alias name");
        add_semantic_token(ctx, name.range, Semantic::Type);
        auto const equals_sign = require_extract(ctx, lex::Type::Equals);
        add_semantic_token(ctx, equals_sign.range, Semantic::Operator_name);
        return cst::expr::Type_alias {
            .name              = name,
            .type              = require<parse_type>(ctx, "an aliased type"),
            .alias_token       = token(ctx, alias_keyword),
            .equals_sign_token = token(ctx, equals_sign),
        };
    }

    auto extract_sizeof(Context& ctx, lex::Token const& sizeof_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, sizeof_keyword.range);
        return cst::expr::Sizeof {
            .type = require<parse_parenthesized<parse_type, "a type">>(ctx, "a parenthesized type"),
            .sizeof_token = token(ctx, sizeof_keyword),
        };
    }

    auto parse_match_arm(Context& ctx) -> std::optional<cst::Match_arm>
    {
        return parse_top_level_pattern(ctx).transform([&](cst::Pattern_id const pattern) {
            auto const arrow = require_extract(ctx, lex::Type::Right_arrow);
            add_semantic_token(ctx, arrow.range, Semantic::Operator_name);
            auto const handler   = require<parse_expression>(ctx, "an expression");
            auto const semicolon = try_extract(ctx, lex::Type::Semicolon);
            return cst::Match_arm {
                .pattern         = pattern,
                .handler         = handler,
                .arrow_token     = token(ctx, arrow),
                .semicolon_token = semicolon.transform(std::bind_front(token, std::ref(ctx))),
            };
        });
    }

    auto parse_match_arms(Context& ctx) -> std::optional<std::vector<cst::Match_arm>>
    {
        std::vector<cst::Match_arm> arms;
        while (auto arm = parse_match_arm(ctx)) {
            arms.push_back(std::move(arm).value());
        }
        return not arms.empty() ? std::optional(std::move(arms)) : std::nullopt;
    }

    auto extract_match(Context& ctx, lex::Token const& match_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, match_keyword.range);
        auto const expression = require<parse_expression>(ctx, "an expression");
        return cst::expr::Match {
            .arms = require<parse_braced<parse_match_arms, "one or more match arms">>(
                ctx, "a '{' followed by match arms"),
            .scrutinee   = expression,
            .match_token = token(ctx, match_keyword),
        };
    }

    auto extract_continue(Context& ctx, lex::Token const& continue_keyword)
        -> cst::Expression_variant
    {
        add_keyword(ctx, continue_keyword.range);
        return cst::expr::Continue { token(ctx, continue_keyword) };
    }

    auto extract_break(Context& ctx, lex::Token const& break_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, break_keyword.range);
        return cst::expr::Break {
            .result      = parse_expression(ctx),
            .break_token = token(ctx, break_keyword),
        };
    }

    auto extract_ret(Context& ctx, lex::Token const& ret_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, ret_keyword.range);
        return cst::expr::Return {
            .expression = parse_expression(ctx),
            .ret_token  = token(ctx, ret_keyword),
        };
    }

    auto extract_addressof(Context& ctx, lex::Token const& ampersand) -> cst::Expression_variant
    {
        add_semantic_token(ctx, ampersand.range, Semantic::Operator_name);
        return cst::expr::Addressof {
            .mutability      = parse_mutability(ctx),
            .expression      = require<parse_expression>(ctx, "the referenced expression"),
            .ampersand_token = token(ctx, ampersand),
        };
    }

    auto extract_defer(Context& ctx, lex::Token const& defer_keyword) -> cst::Expression_variant
    {
        add_keyword(ctx, defer_keyword.range);
        return cst::expr::Defer {
            .expression  = require<parse_expression>(ctx, "an expression"),
            .defer_token = token(ctx, defer_keyword),
        };
    }

    auto extract_block_expression(Context& ctx, lex::Token const& brace_open)
        -> cst::Expression_variant
    {
        std::vector<cst::expr::Block::Side_effect> side_effects;
        std::optional<cst::Expression_id>          result_expression;

        add_punctuation(ctx, brace_open.range);
        while (auto expression = parse_expression(ctx)) {
            if (auto const semicolon = try_extract(ctx, lex::Type::Semicolon)) {
                add_punctuation(ctx, semicolon.value().range);
                side_effects.push_back(cst::expr::Block::Side_effect {
                    .expression               = expression.value(),
                    .trailing_semicolon_token = token(ctx, semicolon.value()),
                });
            }
            else {
                result_expression = expression.value();
                break;
            }
        }

        auto const brace_close = require_extract(ctx, lex::Type::Brace_close);
        add_punctuation(ctx, brace_close.range);
        return cst::expr::Block {
            .effects           = std::move(side_effects),
            .result            = std::move(result_expression),
            .open_brace_token  = token(ctx, brace_open),
            .close_brace_token = token(ctx, brace_close),
        };
    }

    auto try_extract_initializer(Context& ctx, cst::Path&& path) -> cst::Expression_variant
    {
        static constexpr auto parse_braced_initializers = parse_braced<
            parse_comma_separated_one_or_more<
                parse_struct_field_initializer,
                "a field initializer">,
            "one or more field initializers">;
        static constexpr auto parse_parenthesized_initializers = parse_parenthesized<
            parse_comma_separated_one_or_more<parse_expression, "an expression">,
            "one or more expressions">;

        if (db::is_uppercase(ctx.db.string_pool.get(path.head().name.id))) {
            set_previous_path_head_semantic_type(ctx, Semantic::Constructor);

            if (auto initializers = parse_braced_initializers(ctx)) {
                return cst::expr::Struct_init {
                    .path   = std::move(path),
                    .fields = std::move(initializers).value(),
                };
            }
            if (auto initializers = parse_parenthesized_initializers(ctx)) {
                return cst::expr::Tuple_init {
                    .path   = std::move(path),
                    .fields = std::move(initializers).value(),
                };
            }
        }
        return path;
    }

    auto dispatch_parse_normal_expression(Context& ctx) -> std::optional<cst::Expression_variant>
    {
        switch (peek(ctx).type) {
        case lex::Type::Underscore:   return cst::Wildcard { token(ctx, extract(ctx)) };
        case lex::Type::Boolean:      return parse_boolean(ctx, extract(ctx));
        case lex::Type::String:       return parse_string(ctx, extract(ctx));
        case lex::Type::Integer:      return parse_integer(ctx, extract(ctx));
        case lex::Type::Floating:     return parse_floating(ctx, extract(ctx));
        case lex::Type::Asterisk:     return extract_dereference(ctx, extract(ctx));
        case lex::Type::Paren_open:   return extract_tuple(ctx, extract(ctx));
        case lex::Type::Bracket_open: return extract_array(ctx, extract(ctx));
        case lex::Type::If:           return extract_conditional(ctx, extract(ctx));
        case lex::Type::Let:          return extract_let(ctx, extract(ctx));
        case lex::Type::Alias:        return extract_local_type_alias(ctx, extract(ctx));
        case lex::Type::Loop:         return extract_plain_loop(ctx, extract(ctx));
        case lex::Type::While:        return extract_while_loop(ctx, extract(ctx));
        case lex::Type::For:          return extract_for_loop(ctx, extract(ctx));
        case lex::Type::Sizeof:       return extract_sizeof(ctx, extract(ctx));
        case lex::Type::Match:        return extract_match(ctx, extract(ctx));
        case lex::Type::Continue:     return extract_continue(ctx, extract(ctx));
        case lex::Type::Break:        return extract_break(ctx, extract(ctx));
        case lex::Type::Ret:          return extract_ret(ctx, extract(ctx));
        case lex::Type::Ampersand:    return extract_addressof(ctx, extract(ctx));
        case lex::Type::Defer:        return extract_defer(ctx, extract(ctx));
        case lex::Type::Brace_open:   return extract_block_expression(ctx, extract(ctx));
        default:
            return parse_complex_path(ctx).transform(
                [&](cst::Path&& path) { return try_extract_initializer(ctx, std::move(path)); });
        }
    }

    auto parse_normal_expression(Context& ctx) -> std::optional<cst::Expression_id>
    {
        auto const anchor_range = peek(ctx).range;
        return dispatch_parse_normal_expression(ctx).transform(
            [&](cst::Expression_variant&& variant) {
                return ctx.arena.expressions.push(
                    std::move(variant), up_to_current(ctx, anchor_range));
            });
    }

    auto parse_potential_function_call(Context& ctx) -> std::optional<cst::Expression_id>
    {
        return parse_normal_expression(ctx).transform([&](cst::Expression_id expression) {
            while (auto arguments = parse_function_arguments(ctx)) {
                expression = ctx.arena.expressions.push(
                    cst::expr::Function_call {
                        .arguments = std::move(arguments).value(),
                        .invocable = expression,
                    },
                    up_to_current(ctx, ctx.arena.ranges[ctx.arena.expressions[expression].range]));
            }
            return expression;
        });
    }

    auto extract_struct_field_access(
        db::Lower const          field_name,
        cst::Range_id const      dot_token,
        cst::Expression_id const expression,
        Context&                 ctx) -> cst::Expression_variant
    {
        if (peek(ctx).type == lex::Type::Bracket_open or peek(ctx).type == lex::Type::Paren_open) {
            add_semantic_token(ctx, field_name.range, Semantic::Method);
        }
        else {
            add_semantic_token(ctx, field_name.range, Semantic::Property);
        }

        auto template_arguments = parse_template_arguments(ctx);
        if (auto arguments = parse_function_arguments(ctx)) {
            return cst::expr::Method_call {
                .function_arguments = std::move(arguments).value(),
                .template_arguments = std::move(template_arguments),
                .base_expression    = expression,
                .method_name        = field_name,
            };
        }
        if (template_arguments.has_value()) {
            error_expected(ctx, "a parenthesized argument set");
        }
        return cst::expr::Struct_field {
            .base_expression = expression,
            .name            = field_name,
            .dot_token       = dot_token,
        };
    }

    auto extract_member_access(cst::Range_id dot_token, cst::Expression_id base, Context& ctx)
        -> cst::Expression_variant
    {
        if (auto const name = parse_lower_name(ctx)) {
            return extract_struct_field_access(name.value(), dot_token, base, ctx);
        }
        if (auto const index = parse_bracketed<parse_expression, "an index expression">(ctx)) {
            return cst::expr::Array_index {
                .base_expression  = base,
                .index_expression = index.value(),
                .dot_token        = dot_token,
            };
        }
        if (auto const literal = try_extract(ctx, lex::Type::Integer)) {
            add_semantic_token(ctx, literal.value().range, Semantic::Number);
            if (auto const index = parse_integer(ctx, literal.value())) {
                if (std::in_range<std::uint16_t>(index.value().value)) {
                    return cst::expr::Tuple_field {
                        .base_expression   = base,
                        .field_index       = static_cast<std::uint16_t>(index.value().value),
                        .field_index_token = token(ctx, literal.value()),
                        .dot_token         = dot_token,
                    };
                }
            }
            return db::Error {};
        }
        error_expected(ctx, "a struct member name, a tuple member index, or an array index");
    }

    auto parse_potential_member_access(Context& ctx) -> std::optional<cst::Expression_id>
    {
        return parse_potential_function_call(ctx).transform([&](cst::Expression_id expression) {
            while (auto const dot = try_extract(ctx, lex::Type::Dot)) {
                add_semantic_token(ctx, dot.value().range, Semantic::Operator_name);
                expression = ctx.arena.expressions.push(
                    extract_member_access(token(ctx, dot.value()), expression, ctx),
                    up_to_current(ctx, ctx.arena.ranges[ctx.arena.expressions[expression].range]));
            }
            return expression;
        });
    }

    auto parse_potential_type_ascriptions(Context& ctx) -> std::optional<cst::Expression_id>
    {
        return parse_potential_member_access(ctx).transform([&](cst::Expression_id expression) {
            while (auto const colon = try_extract(ctx, lex::Type::Colon)) {
                expression = ctx.arena.expressions.push(
                    cst::expr::Ascription {
                        .base_expression = expression,
                        .colon_token     = token(ctx, colon.value()),
                        .type            = require<parse_type>(ctx, "the ascribed type"),
                    },
                    up_to_current(ctx, ctx.arena.ranges[ctx.arena.expressions[expression].range]));
            }
            return expression;
        });
    }

    auto operator_id(Context& ctx, lex::Token const& token) -> std::optional<utl::String_id>
    {
        switch (token.type) {
        case lex::Type::Operator: return identifier(ctx, token);
        case lex::Type::Asterisk: return ctx.asterisk_id;
        case lex::Type::Plus:     return ctx.plus_id;
        default:                  return std::nullopt;
        }
    }

    constexpr std::tuple operators_tuple {
        std::to_array<std::string_view>({ ":=", "+=", "*=", "/=", "%=" }),
        std::to_array<std::string_view>({ "&&", "||" }),
        std::to_array<std::string_view>({ "<", "<=", ">=", ">" }),
        std::to_array<std::string_view>({ "?=", "!=" }),
        std::to_array<std::string_view>({ "+", "-" }),
        std::to_array<std::string_view>({ "*", "/", "%" }),
    };

    constexpr auto operator_table = std::to_array<std::span<std::string_view const>>({
        std::get<0>(operators_tuple),
        std::get<1>(operators_tuple),
        std::get<2>(operators_tuple),
        std::get<3>(operators_tuple),
        std::get<4>(operators_tuple),
        std::get<5>(operators_tuple),
    });

    auto is_precedence(std::size_t precedence, std::string_view op) -> bool
    {
        return precedence == operator_table.size()
            or std::ranges::contains(operator_table.at(precedence), op);
    }

    auto parse_infix_chain(Context& ctx, std::size_t const level)
        -> std::optional<cst::Expression_id>
    {
        if (level == operator_table.size() + 1) {
            return parse_potential_type_ascriptions(ctx);
        }
        return parse_infix_chain(ctx, level + 1).transform([&](cst::Expression_id expression) {
            for (;;) {
                auto op = peek(ctx);
                auto id = operator_id(ctx, op);
                if (not id.has_value()
                    or is_precedence(level, ctx.db.string_pool.get(id.value()))) {
                    return expression;
                }
                (void)extract(ctx); // Skip the infix operator
                if (auto const rhs = parse_infix_chain(ctx, level + 1)) {
                    expression = ctx.arena.expressions.push(
                        cst::expr::Infix_call {
                            .left  = expression,
                            .right = rhs.value(),
                            .op    = db::Name { .id = id.value(), .range = op.range },
                        },
                        ctx.arena.ranges.push(op.range));
                }
                else {
                    error_expected(ctx, "an operand");
                }
            }
        });
    }
} // namespace

auto ki::par::parse_block_expression(Context& ctx) -> std::optional<cst::Expression_id>
{
    bool is_block = peek(ctx).type == lex::Type::Brace_open;
    return is_block ? parse_normal_expression(ctx) : std::nullopt;
}

auto ki::par::parse_expression(Context& ctx) -> std::optional<cst::Expression_id>
{
    return parse_infix_chain(ctx, 0);
}
