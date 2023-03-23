#include "utl/utilities.hpp"
#include "parser_internals.hpp"


namespace {

    auto extract_qualified_upper_name(ast::Root_qualifier&& root, Parse_context& context)
        -> ast::Type::Variant
    {
        auto* const anchor = context.pointer;
        auto name = extract_qualified(std::move(root), context);

        if (name.primary_name.is_upper) {
            if (auto template_arguments = parse_template_arguments(context)) {
                return ast::type::Template_application {
                    std::move(*template_arguments),
                    std::move(name)
                };
            }
            else {
                return ast::type::Typename { std::move(name) };
            }
        }
        else {
            context.error(
                { anchor, context.pointer },
                "Expected a typename, but found a lowercase identifier"
            );
        }
    }

    auto extract_typename(Parse_context& context)
        -> ast::Type::Variant
    {
        context.retreat();
        return extract_qualified_upper_name({ std::monostate {} }, context);
    }

    auto extract_global_typename(Parse_context& context)
        -> ast::Type::Variant
    {
        return extract_qualified_upper_name({ ast::Root_qualifier::Global{} }, context);
    }

    auto extract_tuple(Parse_context& context)
        -> ast::Type::Variant
    {
        auto types = extract_type_sequence(context);
        context.consume_required(Token::Type::paren_close);
        if (types.size() == 1) {
            return std::move(types.front().value);
        }
        else {
            return ast::type::Tuple { std::move(types) };
        }
    }

    auto extract_array_or_slice(Parse_context& context)
        -> ast::Type::Variant
    {
        auto element_type = extract_type(context);

        auto type = std::invoke([&]() -> ast::Type::Variant {
            if (context.try_consume(Token::Type::semicolon)) {
                if (auto length = parse_expression(context)) {
                    return ast::type::Array {
                        .element_type = utl::wrap(std::move(element_type)),
                        .array_length = utl::wrap(std::move(*length))
                    };
                }
                else {
                    context.error_expected("the array length", "Remove the ';' if a slice type was intended");
                }
            }
            else {
                return ast::type::Slice {
                    .element_type = utl::wrap(std::move(element_type))
                };
            }
        });

        context.consume_required(Token::Type::bracket_close);
        return type;
    }

    auto extract_function(Parse_context& context)
        -> ast::Type::Variant
    {
        if (context.try_consume(Token::Type::paren_open)) {
            auto argument_types = extract_type_sequence(context);
            context.consume_required(Token::Type::paren_close);

            if (context.try_consume(Token::Type::colon)) {
                if (auto return_type = parse_type(context)) {
                    return ast::type::Function {
                        std::move(argument_types),
                        utl::wrap(std::move(*return_type))
                    };
                }
                else {
                    context.error_expected("the function return type");
                }
            }
            else {
                context.error_expected("a ':' followed by the function return type");
            }
        }
        else {
            context.error_expected("a parenthesized list of argument types");
        }
    }

    auto extract_typeof_(Parse_context& context)
        -> ast::Type::Variant
    {
        if (context.try_consume(Token::Type::paren_open)) {
            auto expression = extract_expression(context);
            context.consume_required(Token::Type::paren_close);
            return ast::type::Typeof { utl::wrap(std::move(expression)) };
        }
        else {
            context.error_expected("a parenthesized expression");
        }
    }

    auto extract_instance_of(Parse_context& context)
        -> ast::Type::Variant
    {
        return ast::type::Instance_of { extract_class_references(context) };
    }

    auto extract_reference(Parse_context& context)
        -> ast::Type::Variant
    {
        auto const mutability = extract_mutability(context);
        return ast::type::Reference { utl::wrap(extract_type(context)), mutability };
    }

    auto extract_pointer(Parse_context& context)
        -> ast::Type::Variant
    {
        auto mutability = extract_mutability(context);
        return ast::type::Pointer { utl::wrap(extract_type(context)), std::move(mutability) };
    }


    auto parse_normal_type(Parse_context& context) -> tl::optional<ast::Type::Variant> {
        switch (context.extract().type) {
        case Token::Type::i8_type:  return ast::type::Integer::i8;
        case Token::Type::i16_type: return ast::type::Integer::i16;
        case Token::Type::i32_type: return ast::type::Integer::i32;
        case Token::Type::i64_type: return ast::type::Integer::i64;
        case Token::Type::u8_type:  return ast::type::Integer::u8;
        case Token::Type::u16_type: return ast::type::Integer::u16;
        case Token::Type::u32_type: return ast::type::Integer::u32;
        case Token::Type::u64_type: return ast::type::Integer::u64;

        case Token::Type::floating_type:  return ast::type::Floating {};
        case Token::Type::character_type: return ast::type::Character {};
        case Token::Type::boolean_type:   return ast::type::Boolean {};
        case Token::Type::underscore:     return ast::type::Wildcard {};
        case Token::Type::string_type:    return ast::type::String {};
        case Token::Type::upper_self:     return ast::type::Self {};
        case Token::Type::paren_open:     return extract_tuple(context);
        case Token::Type::bracket_open:   return extract_array_or_slice(context);
        case Token::Type::fn:             return extract_function(context);
        case Token::Type::typeof_:         return extract_typeof_(context);
        case Token::Type::inst:           return extract_instance_of(context);
        case Token::Type::ampersand:      return extract_reference(context);
        case Token::Type::asterisk:       return extract_pointer(context);
        case Token::Type::upper_name:
        case Token::Type::lower_name:     return extract_typename(context);
        case Token::Type::double_colon:   return extract_global_typename(context);

        default:
            context.retreat();
            return tl::nullopt;
        }
    }

}


auto parse_type(Parse_context& context) -> tl::optional<ast::Type> {
    Token const* const type_anchor = context.pointer;

    if (auto type_value = parse_normal_type(context)) {
        ast::Type type {
            .value       = std::move(*type_value),
            .source_view = make_source_view(type_anchor, context.pointer - 1)
        };

        Token* const anchor = context.pointer;

        if (context.try_consume(Token::Type::double_colon)) {
            auto name = extract_qualified({ ast::Root_qualifier::Global {} }, context);

            if (name.primary_name.is_upper) {
                name.root_qualifier.value = utl::wrap(std::move(type));
                auto template_arguments = parse_template_arguments(context);

                type = ast::Type {
                    std::invoke([&]() -> ast::Type::Variant {
                        if (template_arguments) {
                            return ast::type::Template_application {
                                std::move(*template_arguments),
                                std::move(name)
                            };
                        }
                        else {
                            return ast::type::Typename { std::move(name) };
                        }
                    }),
                    make_source_view(anchor, context.pointer - 1)
                };
            }
            else {
                // Not a qualified type, retreat
                context.pointer = anchor;
            }
        }

        return type;
    }
    else {
        return tl::nullopt;
    }
}
