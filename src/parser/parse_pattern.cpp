#include "utl/utilities.hpp"
#include "parser_internals.hpp"


namespace {

    auto extract_wildcard(Parse_context&)
        -> ast::Pattern::Variant
    {
        return ast::pattern::Wildcard {};
    };

    template <class T>
    auto extract_literal(Parse_context& context)
        -> ast::Pattern::Variant
    {
        return ast::pattern::Literal<T> { context.previous().value_as<T>() };
    };

    auto extract_tuple(Parse_context& context)
        -> ast::Pattern::Variant
    {
        auto patterns = extract_comma_separated_zero_or_more<parse_pattern, "a pattern">(context);
        context.consume_required(Token::Type::paren_close);

        if (patterns.size() == 1) {
            return std::move(patterns.front().value);
        }
        else {
            return ast::pattern::Tuple { std::move(patterns) };
        }
    };

    auto extract_slice(Parse_context& context)
        -> ast::Pattern::Variant
    {
        static constexpr auto extract_elements =
            extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">;

        auto patterns = extract_elements(context);

        if (context.try_consume(Token::Type::bracket_close)) {
            return ast::pattern::Slice { std::move(patterns) };
        }
        else if (patterns.empty()) {
            context.error_expected("a slice element pattern or a ']'");
        }
        else {
            context.error_expected("a ',' or a ']'");
        }
    };


    constexpr tl::optional<ast::Pattern> (*parse_constructor_pattern)(Parse_context&) =
        parenthesized<parse_top_level_pattern, "a pattern">;


    auto parse_constructor_name(Parse_context& context) -> tl::optional<ast::Qualified_name> {
        auto* const anchor = context.pointer;

        auto name = std::invoke([&]() -> tl::optional<ast::Qualified_name> {
            switch (context.pointer->type) {
            case Token::Type::lower_name:
            case Token::Type::upper_name:
                return extract_qualified({}, context);
            case Token::Type::double_colon:
                ++context.pointer;
                return extract_qualified({ ast::Root_qualifier::Global {} }, context);
            default:
                if (auto type = parse_type(context)) {
                    return extract_qualified({ utl::wrap(std::move(*type)) }, context);
                }
                else {
                    return tl::nullopt;
                }
            }
        });

        if (name && name->primary_name.is_upper) {
            context.error(
                { anchor, context.pointer },
                "Expected an enum constructor name, but found a capitalized identifier"
            );
        }

        return name;
    }


    auto extract_name(Parse_context& context)
        -> ast::Pattern::Variant
    {
        context.retreat();
        auto mutability = extract_mutability(context);

        tl::optional<compiler::Identifier> identifier;

        if (!mutability.was_explicitly_specified()) {
            if (auto ctor_name = parse_constructor_name(context)) {
                if (!ctor_name->is_unqualified()) {
                    return ast::pattern::Constructor {
                        .constructor_name = std::move(*ctor_name),
                        .payload_pattern  = parse_constructor_pattern(context).transform(utl::wrap)
                    };
                }
                else {
                    identifier = ctor_name->primary_name.identifier;
                }
            }
        }

        if (!identifier) {
            identifier = extract_lower_id(context, "a lowercase identifier");
        }

        return ast::pattern::Name {
            .identifier = std::move(*identifier),
            .mutability = std::move(mutability)
        };
    };

    auto extract_qualified_constructor(Parse_context& context)
        -> ast::Pattern::Variant
    {
        context.retreat();

        if (auto name = parse_constructor_name(context)) {
            return ast::pattern::Constructor {
                .constructor_name = std::move(*name),
                .payload_pattern  = parse_constructor_pattern(context).transform(utl::wrap)
            };
        }
        else {
            utl::todo(); // Unreachable?
        }
    };


    auto parse_normal_pattern(Parse_context& context) -> tl::optional<ast::Pattern::Variant> {
        switch (context.extract().type) {
        case Token::Type::underscore:
            return extract_wildcard(context);
        case Token::Type::integer:
            return extract_literal<utl::Isize>(context);
        case Token::Type::floating:
            return extract_literal<utl::Float>(context);
        case Token::Type::character:
            return extract_literal<utl::Char>(context);
        case Token::Type::boolean:
            return extract_literal<bool>(context);
        case Token::Type::string:
            return extract_literal<compiler::String>(context);
        case Token::Type::paren_open:
            return extract_tuple(context);
        case Token::Type::bracket_open:
            return extract_slice(context);
        case Token::Type::lower_name:
        case Token::Type::mut:
            return extract_name(context);
        case Token::Type::upper_name:
            return extract_qualified_constructor(context);
        default:
            context.retreat();
            return tl::nullopt;
        }
    }


    auto parse_potentially_aliased_pattern(Parse_context& context)
        -> tl::optional<ast::Pattern::Variant>
    {
        if (auto pattern = parse_node<ast::Pattern, parse_normal_pattern>(context)) {
            if (context.try_consume(Token::Type::as)) {
                auto mutability = extract_mutability(context);
                auto identifier = extract_lower_id(context, "a pattern alias");

                return ast::pattern::As {
                    .alias {
                        .identifier = std::move(identifier),
                        .mutability = std::move(mutability)
                    },
                    .aliased_pattern = utl::wrap(std::move(*pattern))
                };
            }
            return std::move(pattern->value);
        }
        else {
            return tl::nullopt;
        }
    }


    auto parse_potentially_guarded_pattern(Parse_context& context)
        -> tl::optional<ast::Pattern::Variant>
    {
        if (auto pattern = parse_node<ast::Pattern, parse_potentially_aliased_pattern>(context)) {
            if (context.try_consume(Token::Type::if_)) {
                if (auto guard = parse_expression(context)) {
                    return ast::pattern::Guarded {
                        .guarded_pattern = utl::wrap(std::move(*pattern)),
                        .guard           = std::move(*guard)
                    };
                }
                else {
                    context.error_expected("a guard expression");
                }
            }
            else {
                return std::move(pattern->value);
            }
        }
        else {
            return tl::nullopt;
        }
    }

}


auto parse_pattern(Parse_context& context) -> tl::optional<ast::Pattern> {
    return parse_node<ast::Pattern, parse_potentially_guarded_pattern>(context);
}