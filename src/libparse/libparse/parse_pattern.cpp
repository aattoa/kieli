#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    auto extract_tuple(Context& context, Token const& paren_open) -> cst::Pattern_variant
    {
        auto patterns = extract_comma_separated_zero_or_more<parse_pattern, "a pattern ">(context);
        Token const paren_close = context.require_extract(Token_type::paren_close);
        if (patterns.elements.size() == 1) {
            return cst::pattern::Parenthesized { {
                .value       = std::move(patterns.elements.front()),
                .open_token  = cst::Token::from_lexical(paren_open),
                .close_token = cst::Token::from_lexical(paren_close),
            } };
        }
        return cst::pattern::Tuple { {
            .value       = std::move(patterns),
            .open_token  = cst::Token::from_lexical(paren_open),
            .close_token = cst::Token::from_lexical(paren_close),
        } };
    };

    auto extract_slice(Context& context, Token const& bracket_open) -> cst::Pattern_variant
    {
        auto patterns
            = extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">(context);
        if (auto const bracket_close = context.try_extract(Token_type::bracket_close)) {
            return cst::pattern::Slice { {
                .value       = std::move(patterns),
                .open_token  = cst::Token::from_lexical(bracket_open),
                .close_token = cst::Token::from_lexical(bracket_close.value()),
            } };
        }
        context.error_expected(
            patterns.elements.empty() ? "a slice element pattern or a ']'" : "a ',' or a ']'");
    };

    auto parse_constructor_name(Context& context) -> std::optional<cst::Path>
    {
        // TODO: cleanup
        switch (context.peek().type) {
        case Token_type::upper_name:
        case Token_type::lower_name: return extract_path(context, std::nullopt);
        case Token_type::global:
        {
            auto const global       = context.extract();
            auto const double_colon = context.require_extract(Token_type::double_colon);
            return extract_path(
                context,
                cst::Path_root {
                    .variant = cst::Path_root_global { cst::Token::from_lexical(global) },
                    .double_colon_token = cst::Token::from_lexical(double_colon),
                    .range              = global.range,
                });
        }
        default:
            return parse_type(context).transform([&](cst::Type_id const type) {
                Token const double_colon = context.require_extract(Token_type::double_colon);
                return extract_path(
                    context,
                    cst::Path_root {
                        .variant            = type,
                        .double_colon_token = cst::Token::from_lexical(double_colon),
                        .range              = context.cst().types[type].range,
                    });
            });
        }
    }

    auto parse_field_pattern(Context& context) -> std::optional<cst::pattern::Field>
    {
        return parse_lower_name(context).transform([&](kieli::Lower const& name) {
            return cst::pattern::Field {
                .name = name,
                .field_pattern
                = context.try_extract(Token_type::equals).transform([&](Token const& equals_sign) {
                      return cst::pattern::Field::Field_pattern {
                          .equals_sign_token = cst::Token::from_lexical(equals_sign),
                          .pattern           = require<parse_pattern>(context, "a field pattern"),
                      };
                  }),
            };
        });
    }

    auto extract_constructor_body(Context& context) -> cst::pattern::Constructor_body
    {
        static constexpr parser auto parse_struct_fields
            = parse_braced<parse_comma_separated_one_or_more<parse_field_pattern, "">, "">;
        static constexpr parser auto parse_tuple_pattern
            = parse_parenthesized<parse_top_level_pattern, "a pattern">;

        if (auto fields = parse_struct_fields(context)) {
            return cst::pattern::Struct_constructor { .fields = std::move(fields.value()) };
        }
        if (auto pattern = parse_tuple_pattern(context)) {
            return cst::pattern::Tuple_constructor { .pattern = std::move(pattern.value()) };
        }
        return cst::pattern::Unit_constructor {};
    }

    auto extract_name(Context& context, Stage const stage) -> cst::Pattern_variant
    {
        // TODO: cleanup
        context.unstage(stage);
        auto mutability = parse_mutability(context);

        std::optional<kieli::Lower> name;
        if (!mutability.has_value()) {
            if (auto constructor_name = parse_constructor_name(context)) {
                if (!constructor_name.value().is_simple_name()) {
                    return cst::pattern::Constructor {
                        .path = std::move(constructor_name.value()),
                        .body = extract_constructor_body(context),
                    };
                }
                cpputil::always_assert(!constructor_name.value().head.is_upper());
                name = kieli::Lower { constructor_name.value().head };
            }
        }
        if (!name.has_value()) {
            name = extract_lower_name(context, "a lowercase identifier");
        }
        return cst::pattern::Name {
            .name       = name.value(),
            .mutability = std::move(mutability),
        };
    };

    auto extract_constructor_path(Context& context, Stage const stage) -> cst::Pattern_variant
    {
        context.unstage(stage);
        return cst::pattern::Constructor {
            .path = require<parse_constructor_name>(context, "a constructor name"),
            .body = extract_constructor_body(context),
        };
    };

    auto extract_abbreviated_constructor(Context& context, Token const& double_colon)
        -> cst::Pattern_variant
    {
        return cst::pattern::Abbreviated_constructor {
            .name               = extract_upper_name(context, "a constructor name"),
            .body               = extract_constructor_body(context),
            .double_colon_token = cst::Token::from_lexical(double_colon),
        };
    }

    auto dispatch_parse_pattern(Context& context, Token const& token, Stage const stage)
        -> std::optional<cst::Pattern_variant>
    {
        switch (token.type) {
        case Token_type::underscore:        return cst::Wildcard { token.range };
        case Token_type::integer_literal:   return token.value_as<kieli::Integer>();
        case Token_type::floating_literal:  return token.value_as<kieli::Floating>();
        case Token_type::character_literal: return token.value_as<kieli::Character>();
        case Token_type::boolean_literal:   return token.value_as<kieli::Boolean>();
        case Token_type::string_literal:    return token.value_as<kieli::String>();
        case Token_type::paren_open:        return extract_tuple(context, token);
        case Token_type::bracket_open:      return extract_slice(context, token);
        case Token_type::lower_name:        [[fallthrough]];
        case Token_type::mut:               return extract_name(context, stage);
        case Token_type::upper_name:        return extract_constructor_path(context, stage);
        case Token_type::double_colon:      return extract_abbreviated_constructor(context, token);
        default:                            context.unstage(stage); return std::nullopt;
        }
    }

    auto parse_potentially_aliased_pattern(Context& context) -> std::optional<cst::Pattern_variant>
    {
        Stage const stage       = context.stage();
        Token const first_token = context.extract();
        return dispatch_parse_pattern(context, first_token, stage)
            .transform([&](cst::Pattern_variant variant) -> cst::Pattern_variant {
                if (auto const as_keyword = context.try_extract(Token_type::as)) {
                    return cst::pattern::Alias {
                        .mutability = parse_mutability(context),
                        .name       = extract_lower_name(context, "a pattern alias"),
                        .pattern    = context.cst().patterns.push(
                            std::move(variant),
                            kieli::Range(first_token.range.start, as_keyword.value().range.stop)),
                        .as_keyword_token = cst::Token::from_lexical(as_keyword.value()),
                    };
                }
                return variant;
            });
    }

    auto parse_potentially_guarded_pattern(Context& context) -> std::optional<cst::Pattern_variant>
    {
        auto const anchor_range = context.peek().range;
        return parse_potentially_aliased_pattern(context).transform(
            [&](cst::Pattern_variant variant) -> cst::Pattern_variant {
                if (auto const if_keyword = context.try_extract(Token_type::if_)) {
                    auto guard = require<parse_expression>(context, "a guard expression");
                    return cst::pattern::Guarded {
                        .guarded_pattern = context.cst().patterns.push(
                            std::move(variant),
                            kieli::Range(anchor_range.start, if_keyword.value().range.stop)),
                        .guard_expression = std::move(guard),
                        .if_keyword_token = cst::Token::from_lexical(if_keyword.value()),
                    };
                }
                return variant;
            });
    }
} // namespace

auto libparse::parse_pattern(Context& context) -> std::optional<cst::Pattern_id>
{
    kieli::Range const anchor_range = context.peek().range;
    return parse_potentially_guarded_pattern(context).transform(
        [&](cst::Pattern_variant&& variant) {
            return context.cst().patterns.push(
                std::move(variant), context.up_to_current(anchor_range));
        });
}

auto libparse::parse_top_level_pattern(Context& context) -> std::optional<cst::Pattern_id>
{
    auto const anchor_range = context.peek().range;
    return parse_comma_separated_one_or_more<parse_pattern, "a pattern">(context).transform(
        [&](cst::Separated_sequence<cst::Pattern_id>&& patterns) {
            if (patterns.elements.size() == 1) {
                return patterns.elements.front();
            }
            return context.cst().patterns.push(
                cst::pattern::Top_level_tuple { std::move(patterns) },
                context.up_to_current(anchor_range));
        });
}
