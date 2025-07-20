#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>

using namespace ki;
using namespace ki::par;

namespace {
    auto extract_tuple(Context& ctx, lex::Token const& open_parenthesis) -> cst::Type_variant
    {
        add_punctuation(ctx, open_parenthesis.range);
        auto       types = extract_comma_separated_zero_or_more<parse_type, "a type">(ctx);
        auto const close_parenthesis = require_extract(ctx, lex::Type::Paren_close);
        add_punctuation(ctx, close_parenthesis.range);
        if (types.elements.size() == 1) {
            return cst::type::Paren { {
                .value       = types.elements.front(),
                .open_token  = open_parenthesis.range,
                .close_token = close_parenthesis.range,
            } };
        }
        return cst::type::Tuple { {
            .value       = std::move(types),
            .open_token  = open_parenthesis.range,
            .close_token = close_parenthesis.range,
        } };
    }

    auto extract_array_or_slice(Context& ctx, lex::Token const& open_bracket) -> cst::Type_variant
    {
        add_punctuation(ctx, open_bracket.range);
        auto const element_type = require<parse_type>(ctx, "the element type");
        if (auto const semicolon = try_extract(ctx, lex::Type::Semicolon)) {
            add_punctuation(ctx, semicolon.value().range);
            if (auto const length = parse_expression(ctx)) {
                auto const close_bracket = require_extract(ctx, lex::Type::Bracket_close);
                add_punctuation(ctx, close_bracket.range);
                return cst::type::Array {
                    .element_type        = element_type,
                    .length              = length.value(),
                    .open_bracket_token  = open_bracket.range,
                    .close_bracket_token = close_bracket.range,
                    .semicolon_token     = semicolon.value().range,
                };
            }
            error_expected(ctx, "the array length; remove the ';' if a slice type was intended");
        }
        auto const close_bracket = require_extract(ctx, lex::Type::Bracket_close);
        add_punctuation(ctx, close_bracket.range);
        return cst::type::Slice { .element_type {
            .value       = element_type,
            .open_token  = open_bracket.range,
            .close_token = close_bracket.range,
        } };
    }

    auto extract_function_type(Context& ctx, lex::Token const& fn_keyword) -> cst::Type_variant
    {
        static constexpr auto extract_parameter_types = require<parse_parenthesized<
            pretend_parse<extract_comma_separated_zero_or_more<parse_type, "a parameter type">>,
            "">>;

        add_keyword(ctx, fn_keyword.range);

        auto param_types = extract_parameter_types(ctx, "a parenthesized list of argument types");
        auto return_type = require<parse_type_annotation>(ctx, "a ':' followed by the return type");

        return cst::type::Function {
            .parameter_types = std::move(param_types),
            .return_type     = std::move(return_type),
            .fn_token        = fn_keyword.range,
        };
    }

    auto extract_typeof(Context& ctx, lex::Token const& typeof_keyword) -> cst::Type_variant
    {
        static constexpr auto extract_inspected
            = require<parse_parenthesized<parse_expression, "the inspected expression">>;
        add_keyword(ctx, typeof_keyword.range);
        return cst::type::Typeof {
            .expression   = extract_inspected(ctx, "a parenthesized expression"),
            .typeof_token = typeof_keyword.range,
        };
    }

    auto extract_implementation_type(Context& ctx, lex::Token const& impl_keyword)
        -> cst::Type_variant
    {
        add_keyword(ctx, impl_keyword.range);
        return cst::type::Impl {
            .concepts   = extract_concept_references(ctx),
            .impl_token = impl_keyword.range,
        };
    }

    auto extract_reference(Context& ctx, lex::Token const& ampersand) -> cst::Type_variant
    {
        add_semantic_token(ctx, ampersand.range, Semantic::Operator_name);
        return cst::type::Reference {
            .mutability      = parse_mutability(ctx),
            .referenced_type = require<parse_type>(ctx, "the referenced type"),
            .ampersand_token = ampersand.range,
        };
    }

    auto extract_pointer(Context& ctx, lex::Token const& asterisk) -> cst::Type_variant
    {
        add_semantic_token(ctx, asterisk.range, Semantic::Operator_name);
        return cst::type::Pointer {
            .mutability     = parse_mutability(ctx),
            .pointee_type   = require<parse_type>(ctx, "the pointee type"),
            .asterisk_token = asterisk.range,
        };
    }

    auto dispatch_parse_type(Context& ctx) -> std::optional<cst::Type_variant>
    {
        switch (peek(ctx).type) {
        case lex::Type::Underscore:   return cst::Wildcard { extract(ctx).range };
        case lex::Type::Exclamation:  return cst::type::Never { extract(ctx).range };
        case lex::Type::Paren_open:   return extract_tuple(ctx, extract(ctx));
        case lex::Type::Bracket_open: return extract_array_or_slice(ctx, extract(ctx));
        case lex::Type::Fn:           return extract_function_type(ctx, extract(ctx));
        case lex::Type::Typeof:       return extract_typeof(ctx, extract(ctx));
        case lex::Type::Impl:         return extract_implementation_type(ctx, extract(ctx));
        case lex::Type::Ampersand:    return extract_reference(ctx, extract(ctx));
        case lex::Type::Asterisk:     return extract_pointer(ctx, extract(ctx));
        default:                      return parse_simple_path(ctx);
        }
    }
} // namespace

auto ki::par::parse_type_root(Context& ctx) -> std::optional<cst::Type_id>
{
    auto const anchor_range = peek(ctx).range;
    return dispatch_parse_type(ctx).transform([&](cst::Type_variant&& variant) {
        return ctx.arena.types.push(std::move(variant), up_to_current(ctx, anchor_range));
    });
}

auto ki::par::parse_type(Context& ctx) -> std::optional<cst::Type_id>
{
    return parse_type_root(ctx).transform([&](cst::Type_id const type_id) {
        if (peek(ctx).type != lex::Type::Double_colon) {
            return type_id;
        }
        cst::Path  path  = extract_path(ctx, type_id);
        lsp::Range range = ctx.arena.types[type_id].range;
        return ctx.arena.types.push(std::move(path), up_to_current(ctx, range));
    });
}
