#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    auto extract_definition_sequence(Context& context)
        -> cst::Surrounded<std::vector<cst::Definition>>
    {
        Token const open = context.require_extract(Token_type::brace_open);

        std::vector<cst::Definition> definitions;
        while (auto definition = parse_definition(context)) {
            definitions.push_back(std::move(definition.value()));
        }

        Token const close = context.require_extract(Token_type::brace_close);
        return {
            .value       = std::move(definitions),
            .open_token  = context.token(open),
            .close_token = context.token(close),
        };
    }

    auto extract_function_signature(Context& context, Token const& fn_keyword)
        -> cst::Function_signature
    {
        auto const name                = extract_lower_name(context, "a function name");
        auto       template_parameters = parse_template_parameters(context);
        auto       function_parameters = parse_function_parameters(context);

        if (!function_parameters.has_value()) {
            context.error_expected("a '(' followed by a function parameter list");
        }

        return cst::Function_signature {
            .template_parameters = std::move(template_parameters),
            .function_parameters = std::move(function_parameters.value()),
            .return_type         = parse_type_annotation(context),
            .name                = name,
            .fn_keyword_token    = context.token(fn_keyword),
        };
    }

    auto extract_function(Context& context, Token const& fn_keyword) -> cst::Definition_variant
    {
        auto signature   = extract_function_signature(context, fn_keyword);
        auto equals_sign = context.try_extract(Token_type::equals);

        auto const function_body
            = equals_sign.has_value() ? parse_expression(context) : parse_block_expression(context);

        if (!function_body.has_value()) {
            context.error_expected(
                equals_sign.has_value() ? "the function body expression"
                                        : "the function body: '=' or '{'");
        }

        return cst::definition::Function {
            .signature                  = std::move(signature),
            .body                       = function_body.value(),
            .optional_equals_sign_token = equals_sign.transform( //
                std::bind_front(&Context::token, std::ref(context))),
            .fn_keyword_token           = context.token(fn_keyword),
        };
    }

    auto parse_field(Context& context) -> std::optional<cst::definition::Field>
    {
        return parse_lower_name(context).transform([&](kieli::Lower const& name) {
            return cst::definition::Field {
                .name  = name,
                .type  = require<parse_type_annotation>(context, "a ':' followed by a type"),
                .range = context.up_to_current(name.range),
            };
        });
    }

    auto extract_constructor_body(Context& context) -> cst::definition::Constructor_body
    {
        static constexpr parser auto parse_struct_constructor = parse_braced<
            parse_comma_separated_one_or_more<parse_field, "a field name">,
            "one or more fields">;
        static constexpr parser auto parse_tuple_constructor = parse_parenthesized<
            parse_comma_separated_one_or_more<parse_type, "a type">,
            "one or more types">;

        if (auto fields = parse_struct_constructor(context)) {
            return cst::definition::Struct_constructor {
                .fields = std::move(fields.value()),
            };
        }
        if (auto types = parse_tuple_constructor(context)) {
            return cst::definition::Tuple_constructor {
                .types = std::move(types.value()),
            };
        }
        return cst::definition::Unit_constructor {};
    }

    auto parse_constructor(Context& context) -> std::optional<cst::definition::Constructor>
    {
        return parse_upper_name(context).transform([&](kieli::Upper const& name) {
            return cst::definition::Constructor {
                .name = name,
                .body = extract_constructor_body(context),
            };
        });
    }

    auto extract_structure(Context& context, Token const& struct_keyword) -> cst::Definition_variant
    {
        auto const name = extract_upper_name(context, "a struct name");
        return cst::definition::Struct {
            .template_parameters  = parse_template_parameters(context),
            .body                 = extract_constructor_body(context),
            .name                 = name,
            .struct_keyword_token = context.token(struct_keyword),
        };
    }

    auto extract_enumeration(Context& context, Token const& enum_keyword) -> cst::Definition_variant
    {
        static constexpr auto extract_constructors = require<parse_separated_one_or_more<
            parse_constructor,
            "an enum constructor",
            Token_type::pipe>>;

        auto const name                = extract_upper_name(context, "an enum name");
        auto       template_parameters = parse_template_parameters(context);
        auto const equals_sign         = context.require_extract(Token_type::equals);

        return cst::definition::Enum {
            .template_parameters = std::move(template_parameters),
            .constructors        = extract_constructors(context, "one or more enum constructors"),
            .name                = name,
            .enum_keyword_token  = context.token(enum_keyword),
            .equals_sign_token   = context.token(equals_sign),
        };
    }

    auto extract_type_signature(Context& context, Token const& alias_keyword) -> cst::Type_signature
    {
        auto const name                = extract_upper_name(context, "an alias name");
        auto       template_parameters = parse_template_parameters(context);

        cst::Type_signature signature {
            .template_parameters = std::move(template_parameters),
            .name                = name,
            .alias_keyword_token = context.token(alias_keyword),
        };
        if (auto const colon_token = context.try_extract(Token_type::colon)) {
            signature.concepts_colon_token = context.token(colon_token.value());
            signature.concepts             = extract_concept_references(context);
        }
        return signature;
    }

    auto extract_concept(Context& context, Token const& concept_keyword) -> cst::Definition_variant
    {
        auto const name                = extract_upper_name(context, "a concept name");
        auto       template_parameters = parse_template_parameters(context);
        auto const open_brace          = context.require_extract(Token_type::brace_open);

        std::vector<cst::Concept_requirement> requirements;

        for (;;) {
            switch (context.peek().type) {
            case Token_type::fn:
                requirements.emplace_back(extract_function_signature(context, context.extract()));
                continue;
            case Token_type::alias:
                requirements.emplace_back(extract_type_signature(context, context.extract()));
                continue;
            default:
                Token const close_brace = context.require_extract(Token_type::brace_close);
                return cst::definition::Concept {
                    .template_parameters   = std::move(template_parameters),
                    .requirements          = std::move(requirements),
                    .name                  = name,
                    .concept_keyword_token = context.token(concept_keyword),
                    .open_brace_token      = context.token(open_brace),
                    .close_brace_token     = context.token(close_brace),
                };
            }
        }
    }

    auto extract_alias(Context& context, Token const& alias_keyword) -> cst::Definition_variant
    {
        auto const name                = extract_upper_name(context, "an alias name");
        auto       template_parameters = parse_template_parameters(context);
        auto const equals_sign         = context.require_extract(Token_type::equals);

        return cst::definition::Alias {
            .template_parameters = std::move(template_parameters),
            .name                = name,
            .type                = require<parse_type>(context, "the aliased type"),
            .alias_keyword_token = context.token(alias_keyword),
            .equals_sign_token   = context.token(equals_sign),
        };
    }

    auto extract_implementation(Context& context, Token const& impl_keyword)
        -> cst::Definition_variant
    {
        auto template_parameters = parse_template_parameters(context);
        auto self_type           = require<parse_type>(context, "the Self type");
        auto definitions         = extract_definition_sequence(context);

        return cst::definition::Implementation {
            .template_parameters = std::move(template_parameters),
            .definitions         = std::move(definitions),
            .self_type           = self_type,
            .impl_keyword_token  = context.token(impl_keyword),
        };
    }

    auto extract_submodule(Context& context, Token const& module_keyword) -> cst::Definition_variant
    {
        auto const name = extract_lower_name(context, "a namespace name");
        return cst::definition::Submodule {
            .template_parameters  = parse_template_parameters(context),
            .definitions          = extract_definition_sequence(context),
            .name                 = name,
            .module_keyword_token = context.token(module_keyword),
        };
    }

    auto dispatch_parse_definition(Context& context) -> std::optional<cst::Definition_variant>
    {
        switch (context.peek().type) {
        case Token_type::fn:       return extract_function(context, context.extract());
        case Token_type::struct_:  return extract_structure(context, context.extract());
        case Token_type::enum_:    return extract_enumeration(context, context.extract());
        case Token_type::concept_: return extract_concept(context, context.extract());
        case Token_type::alias:    return extract_alias(context, context.extract());
        case Token_type::impl:     return extract_implementation(context, context.extract());
        case Token_type::module_:  return extract_submodule(context, context.extract());
        default:                   return std::nullopt;
        }
    }
} // namespace

auto libparse::parse_definition(Context& context) -> std::optional<cst::Definition>
{
    kieli::Range const anchor_range = context.peek().range;
    return dispatch_parse_definition(context).transform([&](cst::Definition_variant&& variant) {
        return cst::Definition {
            .variant = std::move(variant),
            .source  = context.document_id(),
            .range   = context.up_to_current(anchor_range),
        };
    });
}
