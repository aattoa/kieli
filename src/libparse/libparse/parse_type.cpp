#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    auto extract_type_path(Context& context, std::optional<cst::Path_root>&& root)
        -> cst::Type_variant
    {
        auto path = extract_path(context, std::move(root));
        if (!path.head.is_upper()) {
            kieli::fatal_error(
                context.db(),
                context.document_id(),
                context.up_to_current(path.head.range),
                "Expected a type, but found a lowercase name");
        }
        if (auto template_arguments = parse_template_arguments(context)) {
            return cst::type::Template_application {
                .template_arguments = std::move(template_arguments.value()),
                .path               = std::move(path),
            };
        }
        return cst::type::Typename { std::move(path) };
    }

    auto extract_typename(Context& context, Stage const stage) -> cst::Type_variant
    {
        context.unstage(stage);
        return extract_type_path(context, {});
    }

    auto extract_global_typename(Context& context, Token const& global) -> cst::Type_variant
    {
        return extract_type_path(
            context,
            cst::Path_root {
                .variant = cst::Path_root_global { context.token(global) },
                .double_colon_token
                = context.token(context.require_extract(Token_type::double_colon)),
                .range = global.range,
            });
    }

    auto extract_tuple(Context& context, Token const& open_parenthesis) -> cst::Type_variant
    {
        auto        types = extract_comma_separated_zero_or_more<parse_type, "a type">(context);
        Token const close_parenthesis = context.require_extract(Token_type::paren_close);
        if (types.elements.size() == 1) {
            return cst::type::Parenthesized { {
                .value       = types.elements.front(),
                .open_token  = context.token(open_parenthesis),
                .close_token = context.token(close_parenthesis),
            } };
        }
        return cst::type::Tuple { {
            .value       = std::move(types),
            .open_token  = context.token(open_parenthesis),
            .close_token = context.token(close_parenthesis),
        } };
    }

    auto extract_array_or_slice(Context& context, Token const& open_bracket) -> cst::Type_variant
    {
        auto const element_type = require<parse_type>(context, "the element type");

        if (auto const semicolon = context.try_extract(Token_type::semicolon)) {
            if (auto const length = parse_expression(context)) {
                Token const close_bracket = context.require_extract(Token_type::bracket_close);
                return cst::type::Array {
                    .element_type        = element_type,
                    .length              = length.value(),
                    .open_bracket_token  = context.token(open_bracket),
                    .close_bracket_token = context.token(close_bracket),
                    .semicolon_token     = context.token(semicolon.value()),
                };
            }
            context.error_expected("the array length; remove the ';' if a slice type was intended");
        }
        Token const close_bracket = context.require_extract(Token_type::bracket_close);
        return cst::type::Slice { .element_type {
            .value       = element_type,
            .open_token  = context.token(open_bracket),
            .close_token = context.token(close_bracket),
        } };
    }

    auto extract_function(Context& context, Token const& fn_keyword) -> cst::Type_variant
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
            .fn_keyword_token = context.token(fn_keyword),
        };
    }

    auto extract_typeof(Context& context, Token const& typeof_keyword) -> cst::Type_variant
    {
        static constexpr auto extract_inspected
            = require<parse_parenthesized<parse_expression, "the inspected expression">>;
        return cst::type::Typeof {
            .inspected_expression = extract_inspected(context, "a parenthesized expression"),
            .typeof_keyword_token = context.token(typeof_keyword),
        };
    }

    auto extract_implementation(Context& context, Token const& impl_keyword) -> cst::Type_variant
    {
        return cst::type::Implementation {
            .concepts           = extract_concept_references(context),
            .impl_keyword_token = context.token(impl_keyword),
        };
    }

    auto extract_reference(Context& context, Token const& ampersand) -> cst::Type_variant
    {
        return cst::type::Reference {
            .mutability      = parse_mutability(context),
            .referenced_type = require<parse_type>(context, "the referenced type"),
            .ampersand_token = context.token(ampersand),
        };
    }

    auto extract_pointer(Context& context, Token const& asterisk) -> cst::Type_variant
    {
        return cst::type::Pointer {
            .mutability     = parse_mutability(context),
            .pointee_type   = require<parse_type>(context, "the pointee type"),
            .asterisk_token = context.token(asterisk),
        };
    }

    auto dispatch_parse_type(Context& context, Token const& token, Stage const stage)
        -> std::optional<cst::Type_variant>
    {
        switch (token.type) {
        case Token_type::i8_type:        return kieli::type::Integer::i8;
        case Token_type::i16_type:       return kieli::type::Integer::i16;
        case Token_type::i32_type:       return kieli::type::Integer::i32;
        case Token_type::i64_type:       return kieli::type::Integer::i64;
        case Token_type::u8_type:        return kieli::type::Integer::u8;
        case Token_type::u16_type:       return kieli::type::Integer::u16;
        case Token_type::u32_type:       return kieli::type::Integer::u32;
        case Token_type::u64_type:       return kieli::type::Integer::u64;
        case Token_type::floating_type:  return kieli::type::Floating {};
        case Token_type::character_type: return kieli::type::Character {};
        case Token_type::boolean_type:   return kieli::type::Boolean {};
        case Token_type::string_type:    return kieli::type::String {};
        case Token_type::underscore:     return cst::Wildcard { context.token(token) };
        case Token_type::upper_self:     return cst::type::Self { context.token(token) };
        case Token_type::exclamation:    return cst::type::Never { context.token(token) };
        case Token_type::paren_open:     return extract_tuple(context, token);
        case Token_type::bracket_open:   return extract_array_or_slice(context, token);
        case Token_type::fn:             return extract_function(context, token);
        case Token_type::typeof_:        return extract_typeof(context, token);
        case Token_type::impl:           return extract_implementation(context, token);
        case Token_type::ampersand:      return extract_reference(context, token);
        case Token_type::asterisk:       return extract_pointer(context, token);
        case Token_type::upper_name:     [[fallthrough]];
        case Token_type::lower_name:     return extract_typename(context, stage);
        case Token_type::global:         return extract_global_typename(context, token);
        default:                         context.unstage(stage); return std::nullopt;
        }
    }

    auto try_extract_path(Context& context, cst::Type_id const type) -> cst::Type_id
    {
        Stage const stage = context.stage();

        if (auto const double_colon = context.try_extract(Token_type::double_colon)) {
            cst::Path_root root {
                .variant            = type,
                .double_colon_token = context.token(double_colon.value()),
                .range              = context.cst().types[type].range,
            };
            cst::Path path = extract_path(context, std::move(root));

            if (path.head.is_upper()) {
                return context.cst().types.push(
                    std::invoke([&]() -> cst::Type_variant {
                        if (auto template_arguments = parse_template_arguments(context)) {
                            return cst::type::Template_application {
                                .template_arguments = std::move(template_arguments.value()),
                                .path               = std::move(path),
                            };
                        }
                        return cst::type::Typename { std::move(path) };
                    }),
                    context.up_to_current(context.cst().types[type].range));
            }
            // Not a type path, retreat
            context.unstage(stage);
        }
        return type;
    }
} // namespace

auto libparse::parse_type(Context& context) -> std::optional<cst::Type_id>
{
    Stage const stage       = context.stage();
    Token const first_token = context.extract();
    return dispatch_parse_type(context, first_token, stage)
        .transform([&](cst::Type_variant&& variant) -> cst::Type_id {
            kieli::Range const range = context.up_to_current(first_token.range);
            return try_extract_path(context, context.cst().types.push(std::move(variant), range));
        });
}
