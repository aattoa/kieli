#include <libparse/parser_internals.hpp>
#include <libutl/common/utilities.hpp>

namespace {

    using namespace libparse;

    auto extract_wildcard(Context&) -> cst::Pattern::Variant
    {
        return cst::pattern::Wildcard {};
    };

    template <class T>
    auto extract_literal(Context& context) -> cst::Pattern::Variant
    {
        return context.previous().value_as<T>();
    };

    auto extract_tuple(Context& context) -> cst::Pattern::Variant
    {
        Lexical_token const* const open = context.pointer - 1;
        auto patterns = extract_comma_separated_zero_or_more<parse_pattern, "a pattern">(context);
        Lexical_token const* const close = context.extract_required(Token_type::paren_close);
        if (patterns.elements.size() == 1) {
            return cst::pattern::Parenthesized { {
                .value       = std::move(patterns.elements.front()),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            } };
        }
        return cst::pattern::Tuple { {
            .value       = std::move(patterns),
            .open_token  = cst::Token::from_lexical(open),
            .close_token = cst::Token::from_lexical(close),
        } };
    };

    auto extract_slice(Context& context) -> cst::Pattern::Variant
    {
        static constexpr auto extract_elements
            = extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">;

        Lexical_token const* const open     = context.pointer - 1;
        auto                       patterns = extract_elements(context);

        if (Lexical_token const* const close = context.try_extract(Token_type::bracket_close)) {
            return cst::pattern::Slice { {
                .value       = std::move(patterns),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            } };
        }
        if (patterns.elements.empty()) {
            context.error_expected("a slice element pattern or a ']'");
        }
        else {
            context.error_expected("a ',' or a ']'");
        }
    };

    constexpr auto parse_constructor_pattern = parenthesized<parse_top_level_pattern, "a pattern">;

    auto parse_constructor_name(Context& context) -> std::optional<cst::Qualified_name>
    {
        switch (context.pointer->type) {
        case Token_type::lower_name:
        case Token_type::upper_name:
            return extract_qualified(context, {});
        case Token_type::global:
            ++context.pointer;
            return extract_qualified(
                context,
                cst::Root_qualifier {
                    .value = cst::Root_qualifier::Global {},
                    .double_colon_token
                    = cst::Token::from_lexical(context.extract_required(Token_type::double_colon)),
                });
        default:
            if (auto type = parse_type(context)) {
                return extract_qualified(
                    context,
                    cst::Root_qualifier {
                        .value              = *type,
                        .double_colon_token = cst::Token::from_lexical(
                            context.extract_required(Token_type::double_colon)),
                    });
            }
            return std::nullopt;
        }
    }

    auto extract_name(Context& context) -> cst::Pattern::Variant
    {
        context.retreat();
        auto mutability = parse_mutability(context);

        std::optional<kieli::Name_lower> name;

        if (!mutability.has_value()) {
            if (auto ctor_name = parse_constructor_name(context)) {
                if (!ctor_name->is_unqualified()) {
                    return cst::pattern::Constructor {
                        .constructor_name = std::move(*ctor_name),
                        .payload_pattern  = parse_constructor_pattern(context),
                    };
                }
                name = ctor_name->primary_name.as_lower();
            }
        }

        if (!name) {
            name = extract_lower_name(context, "a lowercase identifier");
        }

        return cst::pattern::Name {
            .name       = std::move(*name),
            .mutability = std::move(mutability),
        };
    };

    auto extract_qualified_constructor(Context& context) -> cst::Pattern::Variant
    {
        context.retreat();
        if (auto name = parse_constructor_name(context)) {
            return cst::pattern::Constructor {
                .constructor_name = std::move(*name),
                .payload_pattern  = parse_constructor_pattern(context),
            };
        }
        utl::unreachable();
    };

    auto extract_abbreviated_constructor(Context& context) -> cst::Pattern::Variant
    {
        Lexical_token const* const double_colon = context.pointer - 1;
        return cst::pattern::Abbreviated_constructor {
            .constructor_name   = extract_lower_name(context, "a constructor name"),
            .payload_pattern    = parse_constructor_pattern(context),
            .double_colon_token = cst::Token::from_lexical(double_colon),
        };
    }

    auto parse_normal_pattern(Context& context) -> std::optional<cst::Pattern::Variant>
    {
        switch (context.extract().type) {
        case Token_type::underscore:
            return extract_wildcard(context);
        case Token_type::integer_literal:
            return extract_literal<kieli::Integer>(context);
        case Token_type::floating_literal:
            return extract_literal<kieli::Floating>(context);
        case Token_type::character_literal:
            return extract_literal<kieli::Character>(context);
        case Token_type::boolean_literal:
            return extract_literal<kieli::Boolean>(context);
        case Token_type::string_literal:
            return extract_literal<kieli::String>(context);
        case Token_type::paren_open:
            return extract_tuple(context);
        case Token_type::bracket_open:
            return extract_slice(context);
        case Token_type::lower_name:
        case Token_type::mut:
            return extract_name(context);
        case Token_type::upper_name:
            return extract_qualified_constructor(context);
        case Token_type::double_colon:
            return extract_abbreviated_constructor(context);
        default:
            context.retreat();
            return std::nullopt;
        }
    }

    auto parse_potentially_aliased_pattern(Context& context) -> std::optional<cst::Pattern::Variant>
    {
        Lexical_token const* const anchor = context.pointer;
        if (auto pattern = parse_normal_pattern(context)) {
            if (Lexical_token const* const as_keyword = context.try_extract(Token_type::as)) {
                auto mutability = parse_mutability(context);
                auto name       = extract_lower_name(context, "a pattern alias");
                return cst::pattern::Alias {
                    .alias_name       = std::move(name),
                    .alias_mutability = std::move(mutability),
                    .aliased_pattern  = context.wrap(cst::Pattern {
                         .value       = std::move(*pattern),
                         .source_view = context.make_source_view(anchor, as_keyword - 1),
                    }),
                    .as_keyword_token = cst::Token::from_lexical(as_keyword),
                };
            }
            return pattern;
        }
        return std::nullopt;
    }

    auto parse_potentially_guarded_pattern(Context& context) -> std::optional<cst::Pattern::Variant>
    {
        Lexical_token const* const anchor = context.pointer;
        if (auto pattern = parse_potentially_aliased_pattern(context)) {
            if (Lexical_token const* const if_keyword = context.try_extract(Token_type::if_)) {
                if (auto guard = parse_expression(context)) {
                    return cst::pattern::Guarded {
                        .guarded_pattern  = context.wrap(cst::Pattern {
                             .value       = std::move(*pattern),
                             .source_view = context.make_source_view(anchor, if_keyword - 1),
                        }),
                        .guard_expression = *guard,
                        .if_keyword_token = cst::Token::from_lexical(if_keyword),
                    };
                }
                context.error_expected("a guard expression");
            }
            return pattern;
        }
        return std::nullopt;
    }

} // namespace

auto libparse::parse_pattern(Context& context) -> std::optional<utl::Wrapper<cst::Pattern>>
{
    return parse_node<cst::Pattern, parse_potentially_guarded_pattern>(context);
}
