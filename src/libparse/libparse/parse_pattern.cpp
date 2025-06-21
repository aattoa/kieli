#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>

using namespace ki;
using namespace ki::par;

namespace {
    auto extract_tuple(Context& ctx, lex::Token const& paren_open) -> cst::Pattern_variant
    {
        auto patterns    = extract_comma_separated_zero_or_more<parse_pattern, "a pattern">(ctx);
        auto paren_close = require_extract(ctx, lex::Type::Paren_close);
        if (patterns.elements.size() == 1) {
            return cst::patt::Paren { {
                .value       = std::move(patterns.elements.front()),
                .open_token  = token(ctx, paren_open),
                .close_token = token(ctx, paren_close),
            } };
        }
        return cst::patt::Tuple { {
            .value       = std::move(patterns),
            .open_token  = token(ctx, paren_open),
            .close_token = token(ctx, paren_close),
        } };
    };

    auto extract_slice(Context& ctx, lex::Token const& bracket_open) -> cst::Pattern_variant
    {
        auto patterns
            = extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">(ctx);
        if (auto const bracket_close = try_extract(ctx, lex::Type::Bracket_close)) {
            return cst::patt::Slice { {
                .value       = std::move(patterns),
                .open_token  = token(ctx, bracket_open),
                .close_token = token(ctx, bracket_close.value()),
            } };
        }
        error_expected(
            ctx, patterns.elements.empty() ? "a slice element pattern or a ']'" : "a ',' or a ']'");
    };

    auto parse_field_pattern(Context& ctx) -> std::optional<cst::patt::Field>
    {
        return parse_lower_name(ctx).transform([&](db::Lower const& name) {
            add_semantic_token(ctx, name.range, Semantic::Property);
            auto equals = try_extract(ctx, lex::Type::Equals).transform([&](lex::Token const& tok) {
                add_punctuation(ctx, tok.range);
                return cst::patt::Equals {
                    .equals_sign_token = token(ctx, tok),
                    .pattern           = require<parse_pattern>(ctx, "a pattern"),
                };
            });
            return cst::patt::Field { .name = name, .equals = std::move(equals) };
        });
    }

    auto extract_constructor_body(Context& ctx) -> cst::patt::Constructor_body
    {
        static constexpr auto parse_struct_fields = parse_braced<
            parse_comma_separated_one_or_more<parse_field_pattern, "">,
            "one or more struct field patterns">;
        static constexpr auto parse_tuple_pattern = parse_parenthesized<
            parse_comma_separated_one_or_more<parse_pattern, "a pattern">,
            "one or more tuple field patterns">;

        set_previous_path_head_semantic_type(ctx, Semantic::Constructor);

        if (auto fields = parse_struct_fields(ctx)) {
            return cst::patt::Struct_constructor { .fields = std::move(fields).value() };
        }
        if (auto fields = parse_tuple_pattern(ctx)) {
            return cst::patt::Tuple_constructor { .fields = std::move(fields).value() };
        }
        return cst::patt::Unit_constructor {};
    }

    auto extract_name(Context& ctx) -> cst::Pattern_variant
    {
        auto mutability = parse_mutability(ctx);

        std::optional<db::Lower> name;
        if (not mutability.has_value()) {
            if (auto path = parse_complex_path(ctx)) {
                if (db::is_uppercase(ctx.db.string_pool.get(path.value().head().name.id))
                    or not path.value().is_unqualified()) {
                    return cst::patt::Constructor {
                        .path = std::move(path).value(),
                        .body = extract_constructor_body(ctx),
                    };
                }
                name = db::Lower { path.value().head().name };
            }
        }
        if (not name.has_value()) {
            name = extract_lower_name(ctx, "a lowercase identifier");
            add_semantic_token(ctx, name.value().range, Semantic::Variable);
        }
        return cst::patt::Name {
            .name       = name.value(),
            .mutability = std::move(mutability),
        };
    };

    auto extract_constructor_path(Context& ctx) -> cst::Pattern_variant
    {
        return cst::patt::Constructor {
            .path = require<parse_complex_path>(ctx, "a constructor name"),
            .body = extract_constructor_body(ctx),
        };
    };

    auto dispatch_parse_pattern(Context& ctx) -> std::optional<cst::Pattern_variant>
    {
        switch (peek(ctx).type) {
        case lex::Type::Underscore:   return cst::Wildcard { token(ctx, extract(ctx)) };
        case lex::Type::String:       return parse_string(ctx, extract(ctx));
        case lex::Type::Integer:      return parse_integer(ctx, extract(ctx));
        case lex::Type::Floating:     return parse_floating(ctx, extract(ctx));
        case lex::Type::Boolean:      return parse_boolean(ctx, extract(ctx));
        case lex::Type::Paren_open:   return extract_tuple(ctx, extract(ctx));
        case lex::Type::Bracket_open: return extract_slice(ctx, extract(ctx));
        case lex::Type::Lower_name:
        case lex::Type::Mut:
        case lex::Type::Immut:        return extract_name(ctx);
        case lex::Type::Upper_name:   return extract_constructor_path(ctx);
        default:                      return std::nullopt;
        }
    }

    auto parse_potentially_guarded_pattern(Context& ctx) -> std::optional<cst::Pattern_variant>
    {
        auto const anchor_range = peek(ctx).range;
        return dispatch_parse_pattern(ctx).transform(
            [&](cst::Pattern_variant variant) -> cst::Pattern_variant {
                if (auto const if_keyword = try_extract(ctx, lex::Type::If)) {
                    auto guard = require<parse_expression>(ctx, "a guard expression");
                    auto range = ctx.arena.ranges.push(
                        lsp::Range(anchor_range.start, if_keyword.value().range.stop));
                    return cst::patt::Guarded {
                        .pattern  = ctx.arena.patterns.push(std::move(variant), range),
                        .guard    = std::move(guard),
                        .if_token = token(ctx, if_keyword.value()),
                    };
                }
                return variant;
            });
    }
} // namespace

auto ki::par::parse_pattern(Context& ctx) -> std::optional<cst::Pattern_id>
{
    auto const anchor_range = peek(ctx).range;
    return parse_potentially_guarded_pattern(ctx).transform([&](cst::Pattern_variant&& variant) {
        return ctx.arena.patterns.push(std::move(variant), up_to_current(ctx, anchor_range));
    });
}

auto ki::par::parse_top_level_pattern(Context& ctx) -> std::optional<cst::Pattern_id>
{
    auto const anchor_range = peek(ctx).range;
    return parse_comma_separated_one_or_more<parse_pattern, "a pattern">(ctx).transform(
        [&](cst::Separated<cst::Pattern_id>&& patterns) {
            if (patterns.elements.size() == 1) {
                return patterns.elements.front();
            }
            return ctx.arena.patterns.push(
                cst::patt::Top_level_tuple { std::move(patterns) },
                up_to_current(ctx, anchor_range));
        });
}
