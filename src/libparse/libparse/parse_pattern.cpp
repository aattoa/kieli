#include <libutl/common/utilities.hpp>
#include <libparse/parser_internals.hpp>

namespace {

    using namespace libparse;

    auto extract_tuple(Context& context, Token const& paren_open) -> cst::Pattern::Variant
    {
        auto patterns = extract_comma_separated_zero_or_more<parse_pattern, "a pattern ">(context);
        Token const paren_close = context.require_extract(Token::Type::paren_close);
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

    auto extract_slice(Context& context, Token const& bracket_open) -> cst::Pattern::Variant
    {
        auto patterns
            = extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">(context);
        if (auto const bracket_close = context.try_extract(Token::Type::bracket_close)) {
            return cst::pattern::Slice { {
                .value       = std::move(patterns),
                .open_token  = cst::Token::from_lexical(bracket_open),
                .close_token = cst::Token::from_lexical(bracket_close.value()),
            } };
        }
        context.error_expected(
            patterns.elements.empty() ? "a slice element pattern or a ']'" : "a ',' or a ']'");
    };

    auto parse_constructor_name(Context& context) -> std::optional<cst::Qualified_name>
    {
        // TODO: cleanup
        switch (context.peek().type) {
        case Token::Type::upper_name:
        case Token::Type::lower_name:
            return extract_qualified_name(context, std::nullopt);
        case Token::Type::global:
        {
            auto const global       = context.extract();
            auto const double_colon = context.require_extract(Token::Type::double_colon);
            return extract_qualified_name(
                context,
                cst::Root_qualifier {
                    .variant = cst::Global_root_qualifier { cst::Token::from_lexical(global) },
                    .double_colon_token = cst::Token::from_lexical(double_colon),
                    .source_range       = global.source_range,
                });
        }
        default:
            return parse_type(context).transform([&](utl::Wrapper<cst::Type> const type) {
                auto const double_colon = context.require_extract(Token::Type::double_colon);
                return extract_qualified_name(
                    context,
                    cst::Root_qualifier {
                        .variant            = type,
                        .double_colon_token = cst::Token::from_lexical(double_colon),
                        .source_range       = type->source_range,
                    });
            });
        }
    }

    auto parse_field_pattern(Context& context) -> std::optional<cst::pattern::Field>
    {
        return parse_lower_name(context).transform([&](kieli::Name_lower const& name) {
            return cst::pattern::Field {
                .name = name,
                .field_pattern
                = context.try_extract(Token::Type::equals).transform([&](Token const& equals_sign) {
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

    auto extract_name(Context& context, Stage const stage) -> cst::Pattern::Variant
    {
        // TODO: cleanup
        context.unstage(stage);
        auto mutability = parse_mutability(context);

        std::optional<kieli::Name_lower> name;
        if (!mutability.has_value()) {
            if (auto constructor_name = parse_constructor_name(context)) {
                if (!constructor_name->is_unqualified()) {
                    return cst::pattern::Constructor {
                        .name = std::move(constructor_name.value()),
                        .body = extract_constructor_body(context),
                    };
                }
                name = constructor_name.value().primary_name.as_lower();
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

    auto extract_qualified_constructor(Context& context, Stage const stage) -> cst::Pattern::Variant
    {
        context.unstage(stage);
        return cst::pattern::Constructor {
            .name = require<parse_constructor_name>(context, "a constructor name"),
            .body = extract_constructor_body(context),
        };
    };

    auto extract_abbreviated_constructor(Context& context, Token const& double_colon)
        -> cst::Pattern::Variant
    {
        return cst::pattern::Abbreviated_constructor {
            .name               = extract_upper_name(context, "a constructor name"),
            .body               = extract_constructor_body(context),
            .double_colon_token = cst::Token::from_lexical(double_colon),
        };
    }

    auto dispatch_parse_pattern(Context& context, Token const& token, Stage const stage)
        -> std::optional<cst::Pattern::Variant>
    {
        // clang-format off
        switch (token.type) {
        case Token::Type::underscore:        return cst::Wildcard { token.source_range };
        case Token::Type::integer_literal:   return token.value_as<kieli::Integer>();
        case Token::Type::floating_literal:  return token.value_as<kieli::Floating>();
        case Token::Type::character_literal: return token.value_as<kieli::Character>();
        case Token::Type::boolean_literal:   return token.value_as<kieli::Boolean>();
        case Token::Type::string_literal:    return token.value_as<kieli::String>();
        case Token::Type::paren_open:        return extract_tuple(context, token);
        case Token::Type::bracket_open:      return extract_slice(context, token);
        case Token::Type::lower_name:        [[fallthrough]];
        case Token::Type::mut:               return extract_name(context, stage);
        case Token::Type::upper_name:        return extract_qualified_constructor(context, stage);
        case Token::Type::double_colon:      return extract_abbreviated_constructor(context, token);
        default:
            context.unstage(stage);
            return std::nullopt;
        }
        // clang-format on
    }

    auto parse_potentially_aliased_pattern(Context& context) -> std::optional<cst::Pattern::Variant>
    {
        Stage const stage       = context.stage();
        Token const first_token = context.extract();
        return dispatch_parse_pattern(context, first_token, stage)
            .transform([&](cst::Pattern::Variant variant) -> cst::Pattern::Variant {
                if (auto const as_keyword = context.try_extract(Token::Type::as)) {
                    return cst::pattern::Alias {
                        .mutability       = parse_mutability(context),
                        .name             = extract_lower_name(context, "a pattern alias"),
                        .pattern          = context.wrap(cst::Pattern {
                            std::move(variant),
                            first_token.source_range.up_to(as_keyword.value().source_range),
                        }),
                        .as_keyword_token = cst::Token::from_lexical(as_keyword.value()),
                    };
                }
                return variant;
            });
    }

    auto parse_potentially_guarded_pattern(Context& context) -> std::optional<cst::Pattern::Variant>
    {
        auto const anchor_source_range = context.peek().source_range;
        return parse_potentially_aliased_pattern(context).transform(
            [&](cst::Pattern::Variant variant) -> cst::Pattern::Variant {
                if (auto const if_keyword = context.try_extract(Token::Type::if_)) {
                    auto guard = require<parse_expression>(context, "a guard expression");
                    return cst::pattern::Guarded {
                        .guarded_pattern  = context.wrap(cst::Pattern {
                            std::move(variant),
                            anchor_source_range.up_to(if_keyword.value().source_range),
                        }),
                        .guard_expression = std::move(guard),
                        .if_keyword_token = cst::Token::from_lexical(if_keyword.value()),
                    };
                }
                return variant;
            });
    }

} // namespace

auto libparse::parse_pattern(Context& context) -> std::optional<utl::Wrapper<cst::Pattern>>
{
    auto const anchor_source_range = context.peek().source_range;
    return parse_potentially_guarded_pattern(context).transform(
        [&](cst::Pattern::Variant&& variant) {
            return context.wrap(cst::Pattern {
                std::move(variant),
                context.up_to_current(anchor_source_range),
            });
        });
}

auto libparse::parse_top_level_pattern(Context& context)
    -> std::optional<utl::Wrapper<cst::Pattern>>
{
    auto const anchor_source_range = context.peek().source_range;
    return parse_comma_separated_one_or_more<parse_pattern, "a pattern">(context).transform(
        [&](cst::Separated_sequence<utl::Wrapper<cst::Pattern>>&& patterns) {
            if (patterns.elements.size() == 1) {
                return patterns.elements.front();
            }
            return context.wrap(cst::Pattern {
                .variant      = cst::pattern::Top_level_tuple { std::move(patterns) },
                .source_range = context.up_to_current(anchor_source_range),
            });
        });
}
