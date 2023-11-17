#include <libparse/parser_internals.hpp>
#include <libutl/common/utilities.hpp>

namespace {

    using namespace libparse;

    auto extract_qualified_typename(
        Parse_context& context, std::optional<cst::Root_qualifier>&& root) -> cst::Type::Variant
    {
        Lexical_token const* const anchor = context.pointer;
        auto                       name   = extract_qualified(context, std::move(root));
        if (!name.is_upper()) {
            context.diagnostics().error(
                context.make_source_view(anchor, context.pointer - 1),
                "Expected a typename, but found a lowercase identifier");
        }
        if (auto template_arguments = parse_template_arguments(context)) {
            return cst::type::Template_application {
                .template_arguments = std::move(*template_arguments),
                .name               = std::move(name),
            };
        }
        return cst::type::Typename { std::move(name) };
    }

    auto extract_typename(Parse_context& context) -> cst::Type::Variant
    {
        context.retreat();
        return extract_qualified_typename(context, {});
    }

    auto extract_global_typename(Parse_context& context) -> cst::Type::Variant
    {
        assert(context.pointer[-1].source_view.string == "global");
        return extract_qualified_typename(
            context,
            cst::Root_qualifier {
                .value              = cst::Root_qualifier::Global {},
                .double_colon_token = cst::Token::from_lexical(
                    context.extract_required(Lexical_token::Type::double_colon)),
            });
    }

    auto extract_tuple(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const open_parenthesis = context.pointer - 1;
        assert(open_parenthesis->source_view.string == "(");
        auto types = extract_comma_separated_zero_or_more<parse_type, "a type">(context);
        Lexical_token const* const close_parenthesis
            = context.extract_required(Lexical_token::Type::paren_close);

        if (types.elements.size() == 1) {
            return cst::type::Parenthesized { {
                .value       = types.elements.front(),
                .open_token  = cst::Token::from_lexical(open_parenthesis),
                .close_token = cst::Token::from_lexical(close_parenthesis),
            } };
        }
        return cst::type::Tuple { {
            .value       = std::move(types),
            .open_token  = cst::Token::from_lexical(open_parenthesis),
            .close_token = cst::Token::from_lexical(close_parenthesis),
        } };
    }

    auto extract_array_or_slice(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const open_bracket = context.pointer - 1;
        assert(open_bracket->source_view.string == "[");
        utl::wrapper auto const element_type = extract_type(context);

        auto type = std::invoke([&]() -> cst::Type::Variant {
            if (Lexical_token const* const semicolon = context.try_extract(Token_type::semicolon)) {
                if (auto length = parse_expression(context)) {
                    Lexical_token const* const close_bracket
                        = context.extract_required(Token_type::bracket_close);
                    return cst::type::Array {
                        .element_type        = element_type,
                        .array_length        = *length,
                        .open_bracket_token  = cst::Token::from_lexical(open_bracket),
                        .close_bracket_token = cst::Token::from_lexical(close_bracket),
                        .semicolon_token     = cst::Token::from_lexical(semicolon),
                    };
                }
                context.error_expected(
                    "the array length; remove the ';' if a slice type was intended");
            }
            Lexical_token const* const close_bracket
                = context.extract_required(Token_type::bracket_close);
            return cst::type::Slice { .element_type {
                .value       = element_type,
                .open_token  = cst::Token::from_lexical(open_bracket),
                .close_token = cst::Token::from_lexical(close_bracket),
            } };
        });

        return type;
    }

    auto extract_function(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const fn_keyword = context.pointer - 1;
        assert(fn_keyword->source_view.string == "fn");
        if (Lexical_token const* const open = context.try_extract(Lexical_token::Type::paren_open))
        {
            auto parameter_types
                = extract_comma_separated_zero_or_more<parse_type, "a parameter type">(context);
            Lexical_token const* const close
                = context.extract_required(Lexical_token::Type::paren_close);

            if (auto return_type_annotation = parse_type_annotation(context)) {
                return cst::type::Function {
                    .parameter_types {
                        .value       = std::move(parameter_types),
                        .open_token  = cst::Token::from_lexical(open),
                        .close_token = cst::Token::from_lexical(close),
                    },
                    .return_type      = std::move(*return_type_annotation),
                    .fn_keyword_token = cst::Token::from_lexical(fn_keyword),
                };
            }
            context.error_expected("a ':' followed by the function return type");
        }
        context.error_expected("a parenthesized list of argument types");
    }

    auto extract_typeof(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const typeof_keyword = context.pointer - 1;
        assert(typeof_keyword->source_view.string == "typeof");
        if (Lexical_token const* const open = context.try_extract(Lexical_token::Type::paren_open))
        {
            utl::wrapper auto          expression = extract_expression(context);
            Lexical_token const* const close
                = context.extract_required(Lexical_token::Type::paren_close);
            return cst::type::Typeof {
                .inspected_expression {
                    .value       = expression,
                    .open_token  = cst::Token::from_lexical(open),
                    .close_token = cst::Token::from_lexical(close),
                },
                .typeof_keyword_token = cst::Token::from_lexical(typeof_keyword),
            };
        }
        context.error_expected("a parenthesized expression");
    }

    auto extract_instance_of(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const inst_keyword = context.pointer - 1;
        assert(inst_keyword->source_view.string == "inst");
        return cst::type::Instance_of {
            .classes            = extract_class_references(context),
            .inst_keyword_token = cst::Token::from_lexical(inst_keyword),
        };
    }

    auto extract_reference(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const ampersand = context.pointer - 1;
        assert(ampersand->source_view.string == "&");
        auto mutability = parse_mutability(context);
        return cst::type::Reference {
            .referenced_type = extract_type(context),
            .mutability      = std::move(mutability),
            .ampersand_token = cst::Token::from_lexical(ampersand),
        };
    }

    auto extract_pointer(Parse_context& context) -> cst::Type::Variant
    {
        Lexical_token const* const asterisk = context.pointer - 1;
        assert(asterisk->source_view.string == "*");
        auto mutability = parse_mutability(context);
        return cst::type::Pointer {
            .pointed_to_type = extract_type(context),
            .mutability      = std::move(mutability),
            .asterisk_token  = cst::Token::from_lexical(asterisk),
        };
    }

    auto parse_normal_type(Parse_context& context) -> std::optional<cst::Type::Variant>
    {
        switch (context.extract().type) {
        case Lexical_token::Type::i8_type:
            return kieli::built_in_type::Integer::i8;
        case Lexical_token::Type::i16_type:
            return kieli::built_in_type::Integer::i16;
        case Lexical_token::Type::i32_type:
            return kieli::built_in_type::Integer::i32;
        case Lexical_token::Type::i64_type:
            return kieli::built_in_type::Integer::i64;
        case Lexical_token::Type::u8_type:
            return kieli::built_in_type::Integer::u8;
        case Lexical_token::Type::u16_type:
            return kieli::built_in_type::Integer::u16;
        case Lexical_token::Type::u32_type:
            return kieli::built_in_type::Integer::u32;
        case Lexical_token::Type::u64_type:
            return kieli::built_in_type::Integer::u64;
        case Lexical_token::Type::floating_type:
            return kieli::built_in_type::Floating {};
        case Lexical_token::Type::character_type:
            return kieli::built_in_type::Character {};
        case Lexical_token::Type::boolean_type:
            return kieli::built_in_type::Boolean {};
        case Lexical_token::Type::string_type:
            return kieli::built_in_type::String {};
        case Lexical_token::Type::underscore:
            return cst::type::Wildcard {};
        case Lexical_token::Type::upper_self:
            return cst::type::Self {};
        case Lexical_token::Type::paren_open:
            return extract_tuple(context);
        case Lexical_token::Type::bracket_open:
            return extract_array_or_slice(context);
        case Lexical_token::Type::fn:
            return extract_function(context);
        case Lexical_token::Type::typeof_:
            return extract_typeof(context);
        case Lexical_token::Type::inst:
            return extract_instance_of(context);
        case Lexical_token::Type::ampersand:
            return extract_reference(context);
        case Lexical_token::Type::asterisk:
            return extract_pointer(context);
        case Lexical_token::Type::upper_name:
        case Lexical_token::Type::lower_name:
            return extract_typename(context);
        case Lexical_token::Type::global:
            return extract_global_typename(context);
        default:
            context.retreat();
            return std::nullopt;
        }
    }

} // namespace

auto libparse::parse_type(Parse_context& context) -> std::optional<utl::Wrapper<cst::Type>>
{
    Lexical_token const* const type_anchor = context.pointer;
    if (auto type_value = parse_normal_type(context)) {
        utl::wrapper auto const type   = context.wrap(cst::Type {
              .value       = std::move(*type_value),
              .source_view = context.make_source_view(type_anchor, context.pointer - 1) });
        Lexical_token* const    anchor = context.pointer;
        if (Lexical_token const* const double_colon
            = context.try_extract(Lexical_token::Type::double_colon))
        {
            auto name = extract_qualified(
                context,
                cst::Root_qualifier {
                    .value              = type,
                    .double_colon_token = cst::Token::from_lexical(double_colon),
                });

            if (name.primary_name.is_upper.get()) {
                auto template_arguments = parse_template_arguments(context);
                return context.wrap(cst::Type {
                    .value       = std::invoke([&]() -> cst::Type::Variant {
                        if (template_arguments.has_value()) {
                            return cst::type::Template_application {
                                      .template_arguments = std::move(*template_arguments),
                                      .name               = std::move(name),
                            };
                        }
                        return cst::type::Typename { std::move(name) };
                    }),
                    .source_view = context.make_source_view(anchor, context.pointer - 1),
                });
            }
            // Not a qualified type, retreat
            context.pointer = anchor;
        }
        return type;
    }
    return std::nullopt;
}
