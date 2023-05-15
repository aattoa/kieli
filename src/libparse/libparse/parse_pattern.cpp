#include <libutl/common/utilities.hpp>
#include <libparse/parser_internals.hpp>


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

        if (patterns.size() == 1)
            return std::move(patterns.front().value);
        else
            return ast::pattern::Tuple { std::move(patterns) };
    };

    auto extract_slice(Parse_context& context)
        -> ast::Pattern::Variant
    {
        static constexpr auto extract_elements =
            extract_comma_separated_zero_or_more<parse_pattern, "an element pattern">;

        auto patterns = extract_elements(context);

        if (context.try_consume(Token::Type::bracket_close))
            return ast::pattern::Slice { std::move(patterns) };
        else if (patterns.empty())
            context.error_expected("a slice element pattern or a ']'");
        else
            context.error_expected("a ',' or a ']'");
    };


    constexpr auto parse_constructor_pattern =
        parenthesized<parse_top_level_pattern, "a pattern">;


    auto parse_constructor_name(Parse_context& context) -> tl::optional<ast::Qualified_name> {
        Token const* const anchor = context.pointer;

        auto name = std::invoke([&]() -> tl::optional<ast::Qualified_name> {
            switch (context.pointer->type) {
            case Token::Type::lower_name:
            case Token::Type::upper_name:
                return extract_qualified({}, context);
            case Token::Type::global:
                ++context.pointer;
                context.consume_required(Token::Type::double_colon);
                return extract_qualified({ ast::Global_root_qualifier {} }, context);
            default:
                if (auto type = parse_type(context))
                    return extract_qualified({ context.wrap(std::move(*type)) }, context);
                else
                    return tl::nullopt;
            }
        });

        if (name && name->primary_name.is_upper.get()) {
            context.error(
                make_source_view(anchor, context.pointer),
                { "Expected an enum constructor name, but found a capitalized identifier" });
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
                        .payload_pattern  = parse_constructor_pattern(context).transform(context.wrap())
                    };
                }
                else {
                    identifier = ctor_name->primary_name.identifier;
                }
            }
        }

        if (!identifier)
            identifier = extract_lower_id(context, "a lowercase identifier");

        return ast::pattern::Name {
            .identifier = std::move(*identifier),
            .mutability = std::move(mutability),
        };
    };

    auto extract_qualified_constructor(Parse_context& context)
        -> ast::Pattern::Variant
    {
        context.retreat();
        if (auto name = parse_constructor_name(context)) {
            return ast::pattern::Constructor {
                .constructor_name = std::move(*name),
                .payload_pattern  = parse_constructor_pattern(context).transform(context.wrap()),
            };
        }
        utl::unreachable();
    };

    auto extract_abbreviated_constructor(Parse_context& context)
        -> ast::Pattern::Variant
    {
        return ast::pattern::Abbreviated_constructor {
            .constructor_name = extract_lower_name(context, "a constructor name"),
            .payload_pattern  = parse_constructor_pattern(context).transform(context.wrap()),
        };
    }


    auto parse_normal_pattern(Parse_context& context) -> tl::optional<ast::Pattern::Variant> {
        switch (context.extract().type) {
        case Token::Type::underscore:
            return extract_wildcard(context);
        case Token::Type::signed_integer:
            return extract_literal<compiler::Signed_integer>(context);
        case Token::Type::unsigned_integer:
            return extract_literal<compiler::Unsigned_integer>(context);
        case Token::Type::integer_of_unknown_sign:
            return extract_literal<compiler::Integer_of_unknown_sign>(context);
        case Token::Type::floating:
            return extract_literal<compiler::Floating>(context);
        case Token::Type::character:
            return extract_literal<compiler::Character>(context);
        case Token::Type::boolean:
            return extract_literal<compiler::Boolean>(context);
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
        case Token::Type::double_colon:
            return extract_abbreviated_constructor(context);
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
                    .aliased_pattern = context.wrap(std::move(*pattern))
                };
            }
            return std::move(pattern->value);
        }
        return tl::nullopt;
    }


    auto parse_potentially_guarded_pattern(Parse_context& context)
        -> tl::optional<ast::Pattern::Variant>
    {
        if (auto pattern = parse_node<ast::Pattern, parse_potentially_aliased_pattern>(context)) {
            if (context.try_consume(Token::Type::if_)) {
                if (auto guard = parse_expression(context)) {
                    return ast::pattern::Guarded {
                        .guarded_pattern = context.wrap(std::move(*pattern)),
                        .guard           = std::move(*guard)
                    };
                }
                context.error_expected("a guard expression");
            }
            return std::move(pattern->value);
        }
        return tl::nullopt;
    }

}


auto parse_pattern(Parse_context& context) -> tl::optional<ast::Pattern> {
    return parse_node<ast::Pattern, parse_potentially_guarded_pattern>(context);
}
