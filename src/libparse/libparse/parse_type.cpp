#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

namespace {

    using namespace libparse;

    auto extract_qualified_typename(Context& context, std::optional<cst::Root_qualifier>&& root)
        -> cst::Type::Variant
    {
        auto name = extract_qualified_name(context, std::move(root));
        if (!name.is_upper()) {
            context.compile_info().diagnostics.error(
                context.source(),
                context.up_to_current(name.primary_name.source_range),
                "Expected a type, but found a lowercase name");
        }
        if (auto template_arguments = parse_template_arguments(context)) {
            return cst::type::Template_application {
                .template_arguments = std::move(template_arguments.value()),
                .name               = std::move(name),
            };
        }
        return cst::type::Typename { std::move(name) };
    }

    auto extract_typename(Context& context, Stage const stage) -> cst::Type::Variant
    {
        context.unstage(stage);
        return extract_qualified_typename(context, {});
    }

    auto extract_global_typename(Context& context, Token const& global) -> cst::Type::Variant
    {
        return extract_qualified_typename(
            context,
            cst::Root_qualifier {
                .variant = cst::Global_root_qualifier { cst::Token::from_lexical(global) },
                .double_colon_token
                = cst::Token::from_lexical(context.require_extract(Token::Type::double_colon)),
                .source_range = global.source_range,
            });
    }

    auto extract_tuple(Context& context, Token const& open_parenthesis) -> cst::Type::Variant
    {
        auto        types = extract_comma_separated_zero_or_more<parse_type, "a type">(context);
        Token const close_parenthesis = context.require_extract(Token::Type::paren_close);
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

    auto extract_array_or_slice(Context& context, Token const& open_bracket) -> cst::Type::Variant
    {
        auto const element_type = require<parse_type>(context, "the element type");

        if (auto const semicolon = context.try_extract(Token::Type::semicolon)) {
            if (auto const length = parse_expression(context)) {
                Token const close_bracket = context.require_extract(Token::Type::bracket_close);
                return cst::type::Array {
                    .element_type        = element_type,
                    .length              = length.value(),
                    .open_bracket_token  = cst::Token::from_lexical(open_bracket),
                    .close_bracket_token = cst::Token::from_lexical(close_bracket),
                    .semicolon_token     = cst::Token::from_lexical(semicolon.value()),
                };
            }
            context.error_expected("the array length; remove the ';' if a slice type was intended");
        }
        Token const close_bracket = context.require_extract(Token::Type::bracket_close);
        return cst::type::Slice { .element_type {
            .value       = element_type,
            .open_token  = cst::Token::from_lexical(open_bracket),
            .close_token = cst::Token::from_lexical(close_bracket),
        } };
    }

    auto extract_function(Context& context, Token const& fn_keyword) -> cst::Type::Variant
    {
        static constexpr auto extract_parameter_types = require<parse_parenthesized<
            pretend_parse<extract_comma_separated_zero_or_more<parse_type, "a parameter type">>,
            "">>;

        auto parameters_types
            = extract_parameter_types(context, "a parenthesized list of argument types");
        auto return_type_annotation
            = require<parse_type_annotation>(context, "a ':' followed by the function return type");

        return cst::type::Function {
            .parameter_types  = std::move(parameters_types),
            .return_type      = std::move(return_type_annotation),
            .fn_keyword_token = cst::Token::from_lexical(fn_keyword),
        };
    }

    auto extract_typeof(Context& context, Token const& typeof_keyword) -> cst::Type::Variant
    {
        static constexpr auto extract_inspected
            = require<parse_parenthesized<parse_expression, "the inspected expression">>;
        return cst::type::Typeof {
            .inspected_expression = extract_inspected(context, "a parenthesized expression"),
            .typeof_keyword_token = cst::Token::from_lexical(typeof_keyword),
        };
    }

    auto extract_instance_of(Context& context, Token const& inst_keyword) -> cst::Type::Variant
    {
        return cst::type::Instance_of {
            .classes            = extract_class_references(context),
            .inst_keyword_token = cst::Token::from_lexical(inst_keyword),
        };
    }

    auto extract_reference(Context& context, Token const& ampersand) -> cst::Type::Variant
    {
        return cst::type::Reference {
            .mutability      = parse_mutability(context),
            .referenced_type = require<parse_type>(context, "the referenced type"),
            .ampersand_token = cst::Token::from_lexical(ampersand),
        };
    }

    auto extract_pointer(Context& context, Token const& asterisk) -> cst::Type::Variant
    {
        return cst::type::Pointer {
            .mutability     = parse_mutability(context),
            .pointee_type   = require<parse_type>(context, "the pointee type"),
            .asterisk_token = cst::Token::from_lexical(asterisk),
        };
    }

    auto dispatch_parse_type(Context& context, Token const& token, Stage const stage)
        -> std::optional<cst::Type::Variant>
    {
        // clang-format off
        switch (token.type) {
        case Token::Type::i8_type:        return kieli::built_in_type::Integer::i8;
        case Token::Type::i16_type:       return kieli::built_in_type::Integer::i16;
        case Token::Type::i32_type:       return kieli::built_in_type::Integer::i32;
        case Token::Type::i64_type:       return kieli::built_in_type::Integer::i64;
        case Token::Type::u8_type:        return kieli::built_in_type::Integer::u8;
        case Token::Type::u16_type:       return kieli::built_in_type::Integer::u16;
        case Token::Type::u32_type:       return kieli::built_in_type::Integer::u32;
        case Token::Type::u64_type:       return kieli::built_in_type::Integer::u64;
        case Token::Type::floating_type:  return kieli::built_in_type::Floating {};
        case Token::Type::character_type: return kieli::built_in_type::Character {};
        case Token::Type::boolean_type:   return kieli::built_in_type::Boolean {};
        case Token::Type::string_type:    return kieli::built_in_type::String {};
        case Token::Type::underscore:     return cst::Wildcard { token.source_range };
        case Token::Type::upper_self:     return cst::type::Self {};
        case Token::Type::paren_open:     return extract_tuple(context, token);
        case Token::Type::bracket_open:   return extract_array_or_slice(context, token);
        case Token::Type::fn:             return extract_function(context, token);
        case Token::Type::typeof_:        return extract_typeof(context, token);
        case Token::Type::inst:           return extract_instance_of(context, token);
        case Token::Type::ampersand:      return extract_reference(context, token);
        case Token::Type::asterisk:       return extract_pointer(context, token);
        case Token::Type::upper_name:     [[fallthrough]];
        case Token::Type::lower_name:     return extract_typename(context, stage);
        case Token::Type::global:         return extract_global_typename(context, token);
        default:
            context.unstage(stage);
            return std::nullopt;
        }
        // clang-format on
    }

    auto try_qualify(Context& context, utl::Wrapper<cst::Type> const type)
        -> utl::Wrapper<cst::Type>
    {
        Stage const stage = context.stage();

        if (auto const double_colon = context.try_extract(Token::Type::double_colon)) {
            auto name = extract_qualified_name(
                context,
                cst::Root_qualifier {
                    .variant            = type,
                    .double_colon_token = cst::Token::from_lexical(double_colon.value()),
                    .source_range       = type->source_range,
                });

            if (name.primary_name.is_upper.get()) {
                return context.wrap(cst::Type {
                    std::invoke([&]() -> cst::Type::Variant {
                        auto template_arguments = parse_template_arguments(context);
                        if (template_arguments.has_value()) {
                            return cst::type::Template_application {
                                .template_arguments = std::move(template_arguments.value()),
                                .name               = std::move(name),
                            };
                        }
                        return cst::type::Typename { std::move(name) };
                    }),
                    context.up_to_current(type->source_range),
                });
            }
            // Not a qualified type, retreat
            context.unstage(stage);
        }
        return type;
    }

} // namespace

auto libparse::parse_type(Context& context) -> std::optional<utl::Wrapper<cst::Type>>
{
    Stage const stage       = context.stage();
    Token const first_token = context.extract();
    return dispatch_parse_type(context, first_token, stage)
        .transform([&](cst::Type::Variant&& variant) -> utl::Wrapper<cst::Type> {
            auto const range = context.up_to_current(first_token.source_range);
            return try_qualify(context, context.wrap(cst::Type { std::move(variant), range }));
        });
}
