#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>

using namespace ki;
using namespace ki::par;

namespace {
    auto extract_definition_sequence(Context& ctx) -> cst::Surrounded<std::vector<cst::Definition>>
    {
        auto const open = require_extract(ctx, lex::Type::Brace_open);
        add_punctuation(ctx, open.range);

        std::vector<cst::Definition> definitions;
        while (auto definition = parse_definition(ctx)) {
            definitions.push_back(std::move(definition).value());
        }

        auto const close = require_extract(ctx, lex::Type::Brace_close);
        add_punctuation(ctx, close.range);
        return {
            .value       = std::move(definitions),
            .open_token  = token(ctx, open),
            .close_token = token(ctx, close),
        };
    }

    auto extract_function_signature(Context& ctx, lex::Token const& fn_keyword)
        -> cst::Function_signature
    {
        add_keyword(ctx, fn_keyword.range);
        auto const name = extract_lower_name(ctx, "a function name");
        add_semantic_token(ctx, name.range, Semantic::Function);

        auto template_parameters = parse_template_parameters(ctx);
        auto function_parameters = parse_function_parameters(ctx);

        if (not function_parameters.has_value()) {
            error_expected(ctx, "a '(' followed by a function parameter list");
        }

        return cst::Function_signature {
            .template_parameters = std::move(template_parameters),
            .function_parameters = std::move(function_parameters).value(),
            .return_type         = parse_type_annotation(ctx),
            .name                = name,
            .fn_token            = token(ctx, fn_keyword),
        };
    }

    auto extract_function(Context& ctx, lex::Token const& fn_keyword) -> cst::Definition_variant
    {
        auto signature   = extract_function_signature(ctx, fn_keyword);
        auto equals_sign = try_extract(ctx, lex::Type::Equals);

        if (equals_sign.has_value()) {
            add_semantic_token(ctx, equals_sign.value().range, Semantic::Operator_name);
        }

        auto const function_body
            = equals_sign.has_value() ? parse_expression(ctx) : parse_block_expression(ctx);

        if (not function_body.has_value()) {
            error_expected(
                ctx,
                equals_sign.has_value() ? "the function body expression"
                                        : "the function body: '=' or '{'");
        }

        return cst::Function {
            .signature         = std::move(signature),
            .body              = function_body.value(),
            .equals_sign_token = equals_sign.transform(std::bind_front(token, std::ref(ctx))),
            .fn_token          = token(ctx, fn_keyword),
        };
    }

    auto parse_field(Context& ctx) -> std::optional<cst::Field>
    {
        return parse_lower_name(ctx).transform([&](db::Lower const& name) {
            add_semantic_token(ctx, name.range, Semantic::Property);
            return cst::Field {
                .name  = name,
                .type  = require<parse_type_annotation>(ctx, "a ':' followed by a type"),
                .range = up_to_current(ctx, name.range),
            };
        });
    }

    auto extract_constructor_body(Context& ctx) -> cst::Constructor_body
    {
        static constexpr auto parse_struct_constructor = parse_braced<
            parse_comma_separated_one_or_more<parse_field, "a field name">,
            "one or more fields">;
        static constexpr auto parse_tuple_constructor = parse_parenthesized<
            parse_comma_separated_one_or_more<parse_type, "a type">,
            "one or more types">;

        if (auto fields = parse_struct_constructor(ctx)) {
            return cst::Struct_constructor { .fields = std::move(fields).value() };
        }
        if (auto types = parse_tuple_constructor(ctx)) {
            return cst::Tuple_constructor { .types = std::move(types).value() };
        }
        return cst::Unit_constructor {};
    }

    auto parse_constructor(Context& ctx) -> std::optional<cst::Constructor>
    {
        return parse_upper_name(ctx).transform([&](db::Upper const& name) {
            add_semantic_token(ctx, name.range, Semantic::Constructor);
            return cst::Constructor {
                .name = name,
                .body = extract_constructor_body(ctx),
            };
        });
    }

    auto extract_structure(Context& ctx, lex::Token const& struct_keyword)
        -> cst::Definition_variant
    {
        add_keyword(ctx, struct_keyword.range);
        auto const name = extract_upper_name(ctx, "a struct name");
        add_semantic_token(ctx, name.range, Semantic::Structure);
        return cst::Struct {
            .template_parameters = parse_template_parameters(ctx),
            .constructor = cst::Constructor { .name = name, .body = extract_constructor_body(ctx) },
            .struct_token = token(ctx, struct_keyword),
        };
    }

    auto extract_enumeration(Context& ctx, lex::Token const& enum_keyword)
        -> cst::Definition_variant
    {
        static constexpr auto extract_constructors = require<
            parse_separated_one_or_more<parse_constructor, "an enum constructor", lex::Type::Pipe>>;

        add_keyword(ctx, enum_keyword.range);
        auto const name = extract_upper_name(ctx, "an enum name");
        add_semantic_token(ctx, name.range, Semantic::Enumeration);

        auto       template_parameters = parse_template_parameters(ctx);
        auto const equals_sign         = require_extract(ctx, lex::Type::Equals);
        add_semantic_token(ctx, equals_sign.range, Semantic::Operator_name);

        return cst::Enum {
            .template_parameters = std::move(template_parameters),
            .constructors        = extract_constructors(ctx, "one or more enum constructors"),
            .name                = name,
            .enum_token          = token(ctx, enum_keyword),
            .equals_sign_token   = token(ctx, equals_sign),
        };
    }

    auto extract_type_signature(Context& ctx, lex::Token const& alias_keyword)
        -> cst::Type_signature
    {
        add_keyword(ctx, alias_keyword.range);
        auto const name = extract_upper_name(ctx, "an alias name");
        add_semantic_token(ctx, name.range, Semantic::Type);
        auto template_parameters = parse_template_parameters(ctx);

        cst::Type_signature signature {
            .template_parameters  = std::move(template_parameters),
            .concepts             = {},
            .name                 = name,
            .concepts_colon_token = std::nullopt,
            .alias_token          = token(ctx, alias_keyword),
        };
        if (auto const colon_token = try_extract(ctx, lex::Type::Colon)) {
            signature.concepts_colon_token = token(ctx, colon_token.value());
            signature.concepts             = extract_concept_references(ctx);
        }
        return signature;
    }

    auto extract_concept(Context& ctx, lex::Token const& concept_keyword) -> cst::Definition_variant
    {
        add_keyword(ctx, concept_keyword.range);
        auto const name = extract_upper_name(ctx, "a concept name");
        add_semantic_token(ctx, name.range, Semantic::Interface);

        auto template_parameters = parse_template_parameters(ctx);
        auto open_brace          = require_extract(ctx, lex::Type::Brace_open);
        add_punctuation(ctx, open_brace.range);

        std::vector<cst::Concept_requirement> requirements;
        for (;;) {
            if (auto const fn_keyword = try_extract(ctx, lex::Type::Fn)) {
                requirements.emplace_back(extract_function_signature(ctx, fn_keyword.value()));
            }
            else if (auto const alias_keyword = try_extract(ctx, lex::Type::Alias)) {
                requirements.emplace_back(extract_type_signature(ctx, alias_keyword.value()));
            }
            else {
                auto const close_brace = require_extract(ctx, lex::Type::Brace_close);
                add_punctuation(ctx, close_brace.range);
                return cst::Concept {
                    .template_parameters = std::move(template_parameters),
                    .requirements        = std::move(requirements),
                    .name                = name,
                    .concept_token       = token(ctx, concept_keyword),
                    .open_brace_token    = token(ctx, open_brace),
                    .close_brace_token   = token(ctx, close_brace),
                };
            }
        }
    }

    auto extract_alias(Context& ctx, lex::Token const& alias_keyword) -> cst::Definition_variant
    {
        add_keyword(ctx, alias_keyword.range);
        auto const name = extract_upper_name(ctx, "an alias name");
        add_semantic_token(ctx, name.range, Semantic::Type);

        auto template_parameters = parse_template_parameters(ctx);
        auto equals_sign         = require_extract(ctx, lex::Type::Equals);
        add_semantic_token(ctx, equals_sign.range, Semantic::Operator_name);

        return cst::Alias {
            .template_parameters = std::move(template_parameters),
            .name                = name,
            .type                = require<parse_type>(ctx, "the aliased type"),
            .alias_token         = token(ctx, alias_keyword),
            .equals_sign_token   = token(ctx, equals_sign),
        };
    }

    auto extract_implementation(Context& ctx, lex::Token const& impl_keyword)
        -> cst::Definition_variant
    {
        add_keyword(ctx, impl_keyword.range);
        auto template_parameters = parse_template_parameters(ctx);
        auto self_type           = require<parse_type>(ctx, "the Self type");
        auto definitions         = extract_definition_sequence(ctx);

        return cst::Impl {
            .template_parameters = std::move(template_parameters),
            .definitions         = std::move(definitions),
            .self_type           = self_type,
            .impl_token          = token(ctx, impl_keyword),
        };
    }

    auto extract_submodule(Context& ctx, lex::Token const& module_keyword)
        -> cst::Definition_variant
    {
        add_keyword(ctx, module_keyword.range);
        auto const name = extract_lower_name(ctx, "a module name");
        add_semantic_token(ctx, name.range, Semantic::Module);
        return cst::Submodule {
            .template_parameters = parse_template_parameters(ctx),
            .definitions         = extract_definition_sequence(ctx),
            .name                = name,
            .module_token        = token(ctx, module_keyword),
        };
    }

    auto dispatch_parse_definition(Context& ctx) -> std::optional<cst::Definition_variant>
    {
        switch (peek(ctx).type) {
        case lex::Type::Fn:      return extract_function(ctx, extract(ctx));
        case lex::Type::Struct:  return extract_structure(ctx, extract(ctx));
        case lex::Type::Enum:    return extract_enumeration(ctx, extract(ctx));
        case lex::Type::Concept: return extract_concept(ctx, extract(ctx));
        case lex::Type::Alias:   return extract_alias(ctx, extract(ctx));
        case lex::Type::Impl:    return extract_implementation(ctx, extract(ctx));
        case lex::Type::Module:  return extract_submodule(ctx, extract(ctx));
        default:                 return std::nullopt;
        }
    }
} // namespace

auto ki::par::parse_definition(Context& ctx) -> std::optional<cst::Definition>
{
    auto const anchor_range = peek(ctx).range;
    return dispatch_parse_definition(ctx).transform([&](cst::Definition_variant&& variant) {
        return cst::Definition {
            .variant = std::move(variant),
            .range   = up_to_current(ctx, anchor_range),
        };
    });
}
