#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>

using namespace ki::parse;

namespace {
    auto extract_tuple(Context& ctx, Token const& paren_open) -> cst::Pattern_variant
    {
        auto patterns    = extract_comma_separated_zero_or_more<parse_pattern, "a pattern">(ctx);
        auto paren_close = require_extract(ctx, lex::Type::Paren_close);
        if (patterns.elements.size() == 1) {
            return cst::pattern::Paren { {
                .value       = std::move(patterns.elements.front()),
                .open_token  = token(ctx, paren_open),
                .close_token = token(ctx, paren_close),
            } };
        }
        return cst::pattern::Tuple { {
            .value       = std::move(patterns),
            .open_token  = token(ctx, paren_open),
            .close_token = token(ctx, paren_close),
        } };
    };

    auto extract_slice(Context& ctx, Token const& bracket_open) -> cst::Pattern_variant
    {
        auto patterns
            = extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">(ctx);
        if (auto const bracket_close = try_extract(ctx, lex::Type::Bracket_close)) {
            return cst::pattern::Slice { {
                .value       = std::move(patterns),
                .open_token  = token(ctx, bracket_open),
                .close_token = token(ctx, bracket_close.value()),
            } };
        }
        error_expected(
            ctx, patterns.elements.empty() ? "a slice element pattern or a ']'" : "a ',' or a ']'");
    };

    auto parse_field_pattern(Context& ctx) -> std::optional<cst::pattern::Field>
    {
        return parse_lower_name(ctx).transform([&](ki::Lower const& name) {
            add_semantic_token(ctx, name.range, Semantic::Property);
            return cst::pattern::Field {
                .name = name,
                .equals
                = try_extract(ctx, lex::Type::Equals).transform([&](Token const& equals_sign) {
                      add_punctuation(ctx, equals_sign.range);
                      return cst::pattern::Equals {
                          .equals_sign_token = token(ctx, equals_sign),
                          .pattern           = require<parse_pattern>(ctx, "a field pattern"),
                      };
                  }),
            };
        });
    }

    auto extract_constructor_body(Context& ctx) -> cst::pattern::Constructor_body
    {
        static constexpr auto parse_struct_fields
            = parse_braced<parse_comma_separated_one_or_more<parse_field_pattern, "">, "">;
        static constexpr auto parse_tuple_pattern
            = parse_parenthesized<parse_top_level_pattern, "a pattern">;

        if (auto fields = parse_struct_fields(ctx)) {
            return cst::pattern::Struct_constructor { .fields = std::move(fields.value()) };
        }
        if (auto pattern = parse_tuple_pattern(ctx)) {
            return cst::pattern::Tuple_constructor { .pattern = std::move(pattern.value()) };
        }
        return cst::pattern::Unit_constructor {};
    }

    auto extract_name(Context& ctx) -> cst::Pattern_variant
    {
        auto mutability = parse_mutability(ctx);

        std::optional<ki::Lower> name;
        if (not mutability.has_value()) {
            if (auto path = parse_complex_path(ctx)) {
                if (ki::is_uppercase(ctx.db.string_pool.get(path.value().head().name.id))
                    or not path.value().is_unqualified()) {
                    return cst::pattern::Constructor {
                        .path = std::move(path.value()),
                        .body = extract_constructor_body(ctx),
                    };
                }
                name = ki::Lower { path.value().head().name };
            }
        }
        if (not name.has_value()) {
            name = extract_lower_name(ctx, "a lowercase identifier");
            add_semantic_token(ctx, name.value().range, Semantic::Variable);
        }
        return cst::pattern::Name {
            .name       = name.value(),
            .mutability = std::move(mutability),
        };
    };

    auto extract_constructor_path(Context& ctx) -> cst::Pattern_variant
    {
        return cst::pattern::Constructor {
            .path = require<parse_complex_path>(ctx, "a constructor name"),
            .body = extract_constructor_body(ctx),
        };
    };

    auto extract_abbreviated_constructor(Context& ctx, Token const& double_colon)
        -> cst::Pattern_variant
    {
        add_punctuation(ctx, double_colon.range);
        return cst::pattern::Abbreviated_constructor {
            .name               = extract_upper_name(ctx, "a constructor name"),
            .body               = extract_constructor_body(ctx),
            .double_colon_token = token(ctx, double_colon),
        };
    }

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
        case lex::Type::Double_colon: return extract_abbreviated_constructor(ctx, extract(ctx));
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
                    return cst::pattern::Guarded {
                        .guarded_pattern = ctx.arena.patt.push(
                            std::move(variant),
                            ki::Range(anchor_range.start, if_keyword.value().range.stop)),
                        .guard_expression = std::move(guard),
                        .if_token         = token(ctx, if_keyword.value()),
                    };
                }
                return variant;
            });
    }
} // namespace

auto ki::parse::parse_pattern(Context& ctx) -> std::optional<cst::Pattern_id>
{
    auto const anchor_range = peek(ctx).range;
    return parse_potentially_guarded_pattern(ctx).transform([&](cst::Pattern_variant&& variant) {
        return ctx.arena.patt.push(std::move(variant), up_to_current(ctx, anchor_range));
    });
}

auto ki::parse::parse_top_level_pattern(Context& ctx) -> std::optional<cst::Pattern_id>
{
    auto const anchor_range = peek(ctx).range;
    return parse_comma_separated_one_or_more<parse_pattern, "a pattern">(ctx).transform(
        [&](cst::Separated<cst::Pattern_id>&& patterns) {
            if (patterns.elements.size() == 1) {
                return patterns.elements.front();
            }
            return ctx.arena.patt.push(
                cst::pattern::Top_level_tuple { std::move(patterns) },
                up_to_current(ctx, anchor_range));
        });
}
