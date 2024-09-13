#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
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

    auto dispatch_parse_type(Context& context) -> std::optional<cst::Type_variant>
    {
        switch (context.peek().type) {
        case Token_type::underscore:   return cst::Wildcard { context.token(context.extract()) };
        case Token_type::exclamation:  return cst::type::Never { context.token(context.extract()) };
        case Token_type::paren_open:   return extract_tuple(context, context.extract());
        case Token_type::bracket_open: return extract_array_or_slice(context, context.extract());
        case Token_type::fn:           return extract_function(context, context.extract());
        case Token_type::typeof_:      return extract_typeof(context, context.extract());
        case Token_type::impl:         return extract_implementation(context, context.extract());
        case Token_type::ampersand:    return extract_reference(context, context.extract());
        case Token_type::asterisk:     return extract_pointer(context, context.extract());
        default:                       return parse_simple_path(context);
        }
    }
} // namespace

auto libparse::parse_type_root(Context& context) -> std::optional<cst::Type_id>
{
    kieli::Range const anchor_range = context.peek().range;
    return dispatch_parse_type(context).transform([&](cst::Type_variant&& variant) {
        return context.cst().types.push(std::move(variant), context.up_to_current(anchor_range));
    });
}

auto libparse::parse_type(Context& context) -> std::optional<cst::Type_id>
{
    return parse_type_root(context).transform([&](cst::Type_id const type_id) {
        if (context.peek().type != Token_type::double_colon) {
            return type_id;
        }
        kieli::Range const range = context.cst().types[type_id].range;
        cst::Path          path  = extract_path(context, type_id);
        return context.cst().types.push(std::move(path), context.up_to_current(range));
    });
}
