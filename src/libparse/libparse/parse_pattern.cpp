#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    auto extract_tuple(Context& context, Token const& paren_open) -> cst::Pattern_variant
    {
        auto patterns = extract_comma_separated_zero_or_more<parse_pattern, "a pattern">(context);
        Token const paren_close = context.require_extract(Token_type::paren_close);
        if (patterns.elements.size() == 1) {
            return cst::pattern::Parenthesized { {
                .value       = std::move(patterns.elements.front()),
                .open_token  = context.token(paren_open),
                .close_token = context.token(paren_close),
            } };
        }
        return cst::pattern::Tuple { {
            .value       = std::move(patterns),
            .open_token  = context.token(paren_open),
            .close_token = context.token(paren_close),
        } };
    };

    auto extract_slice(Context& context, Token const& bracket_open) -> cst::Pattern_variant
    {
        auto patterns
            = extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">(context);
        if (auto const bracket_close = context.try_extract(Token_type::bracket_close)) {
            return cst::pattern::Slice { {
                .value       = std::move(patterns),
                .open_token  = context.token(bracket_open),
                .close_token = context.token(bracket_close.value()),
            } };
        }
        context.error_expected(
            patterns.elements.empty() ? "a slice element pattern or a ']'" : "a ',' or a ']'");
    };

    auto parse_field_pattern(Context& context) -> std::optional<cst::pattern::Field>
    {
        return parse_lower_name(context).transform([&](kieli::Lower const& name) {
            return cst::pattern::Field {
                .name = name,
                .field_pattern
                = context.try_extract(Token_type::equals).transform([&](Token const& equals_sign) {
                      return cst::pattern::Field::Field_pattern {
                          .equals_sign_token = context.token(equals_sign),
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

    auto extract_name(Context& context) -> cst::Pattern_variant
    {
        auto mutability = parse_mutability(context);

        std::optional<kieli::Lower> name;
        if (!mutability.has_value()) {
            if (auto path = parse_complex_path(context)) {
                if (path.value().is_upper() || !path.value().is_unqualified()) {
                    return cst::pattern::Constructor {
                        .path = std::move(path.value()),
                        .body = extract_constructor_body(context),
                    };
                }
                name = kieli::Lower { path.value().head().name };
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

    auto extract_constructor_path(Context& context) -> cst::Pattern_variant
    {
        return cst::pattern::Constructor {
            .path = require<parse_complex_path>(context, "a constructor name"),
            .body = extract_constructor_body(context),
        };
    };

    auto extract_abbreviated_constructor(Context& context, Token const& double_colon)
        -> cst::Pattern_variant
    {
        return cst::pattern::Abbreviated_constructor {
            .name               = extract_upper_name(context, "a constructor name"),
            .body               = extract_constructor_body(context),
            .double_colon_token = context.token(double_colon),
        };
    }

    auto dispatch_parse_pattern(Context& context) -> std::optional<cst::Pattern_variant>
    {
        switch (context.peek().type) {
        case Token_type::underscore:        return cst::Wildcard { context.token(context.extract()) };
        case Token_type::integer_literal:   return context.extract().value_as<kieli::Integer>();
        case Token_type::floating_literal:  return context.extract().value_as<kieli::Floating>();
        case Token_type::character_literal: return context.extract().value_as<kieli::Character>();
        case Token_type::boolean_literal:   return context.extract().value_as<kieli::Boolean>();
        case Token_type::string_literal:    return context.extract().value_as<kieli::String>();
        case Token_type::paren_open:        return extract_tuple(context, context.extract());
        case Token_type::bracket_open:      return extract_slice(context, context.extract());
        case Token_type::lower_name:
        case Token_type::mut:
        case Token_type::immut:             return extract_name(context);
        case Token_type::upper_name:        return extract_constructor_path(context);
        case Token_type::double_colon:
            return extract_abbreviated_constructor(context, context.extract());
        default: return std::nullopt;
        }
    }

    auto parse_potentially_aliased_pattern(Context& context) -> std::optional<cst::Pattern_variant>
    {
        kieli::Range const anchor_range = context.peek().range;
        return dispatch_parse_pattern(context).transform(
            [&](cst::Pattern_variant variant) -> cst::Pattern_variant {
                if (auto const as_keyword = context.try_extract(Token_type::as)) {
                    kieli::Range const range(anchor_range.start, as_keyword.value().range.stop);
                    return cst::pattern::Alias {
                        .mutability       = parse_mutability(context),
                        .name             = extract_lower_name(context, "a pattern alias"),
                        .pattern          = context.cst().patterns.push(std::move(variant), range),
                        .as_keyword_token = context.token(as_keyword.value()),
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
                        .if_keyword_token = context.token(if_keyword.value()),
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
