#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>

using namespace ki;
using namespace ki::par;

namespace {
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
            .fn_token            = fn_keyword.range,
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
            .alias_token          = alias_keyword.range,
        };
        if (auto const colon_token = try_extract(ctx, lex::Type::Colon)) {
            signature.concepts_colon_token = colon_token.value().range;
            signature.concepts             = extract_concept_references(ctx);
        }
        return signature;
    }
} // namespace

auto ki::par::extract_function(Context& ctx, lex::Token const& fn_keyword) -> cst::Function
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
        .equals_sign_token = equals_sign.transform([](auto const& tok) { return tok.range; }),
        .fn_token = fn_keyword.range,
        .range    = up_to_current(ctx, fn_keyword.range),
    };
}

auto ki::par::extract_structure(Context& ctx, lex::Token const& struct_keyword) -> cst::Struct
{
    add_keyword(ctx, struct_keyword.range);
    auto const name = extract_upper_name(ctx, "a struct name");
    add_semantic_token(ctx, name.range, Semantic::Structure);
    return cst::Struct {
        .template_parameters = parse_template_parameters(ctx),
        .constructor  = cst::Constructor { .name = name, .body = extract_constructor_body(ctx) },
        .struct_token = struct_keyword.range,
        .range        = up_to_current(ctx, struct_keyword.range),
    };
}

auto ki::par::extract_enumeration(Context& ctx, lex::Token const& enum_keyword) -> cst::Enum
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
        .enum_token          = enum_keyword.range,
        .equals_sign_token   = equals_sign.range,
        .range               = up_to_current(ctx, enum_keyword.range),
    };
}

auto ki::par::extract_concept(Context& ctx, lex::Token const& concept_keyword) -> cst::Concept
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
                .concept_token       = concept_keyword.range,
                .open_brace_token    = open_brace.range,
                .close_brace_token   = close_brace.range,
                .range               = up_to_current(ctx, concept_keyword.range),
            };
        }
    }
}

auto ki::par::extract_alias(Context& ctx, lex::Token const& alias_keyword) -> cst::Alias
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
        .alias_token         = alias_keyword.range,
        .equals_sign_token   = equals_sign.range,
        .range               = up_to_current(ctx, alias_keyword.range),
    };
}

auto ki::par::extract_implementation(Context& ctx, lex::Token const& impl_keyword)
    -> cst::Impl_begin
{
    add_keyword(ctx, impl_keyword.range);
    auto const self_type  = require<parse_type>(ctx, "the Self type");
    auto const open_brace = require_extract(ctx, lex::Type::Brace_open);
    ++ctx.block_depth;
    return cst::Impl_begin {
        .template_parameters = parse_template_parameters(ctx),
        .self_type           = self_type,
        .impl_token          = impl_keyword.range,
        .open_brace_token    = open_brace.range,
        .range               = up_to_current(ctx, impl_keyword.range),
    };
}

auto ki::par::extract_submodule(Context& ctx, lex::Token const& module_keyword)
    -> cst::Submodule_begin
{
    add_keyword(ctx, module_keyword.range);
    auto const name = extract_lower_name(ctx, "a module name");
    add_semantic_token(ctx, name.range, Semantic::Module);
    auto const open_brace = require_extract(ctx, lex::Type::Brace_open);
    ++ctx.block_depth;
    return cst::Submodule_begin {
        .name             = name,
        .module_token     = module_keyword.range,
        .open_brace_token = open_brace.range,
        .range            = up_to_current(ctx, module_keyword.range),
    };
}

auto ki::par::extract_block_end(Context& ctx, lex::Token const& brace_close) -> cst::Block_end
{
    if (ctx.block_depth == 0) {
        ctx.add_diagnostic(lsp::error(brace_close.range, "Unexpected closing brace"));
        throw Failure {};
    }
    --ctx.block_depth;
    return cst::Block_end { .range = brace_close.range };
}

void ki::par::handle_end_of_input(Context& ctx, lex::Token const& end)
{
    cpputil::always_assert(end.type == lex::Type::End_of_input);

    if (ctx.block_depth != 0) {
        auto message = ctx.block_depth == 1
                         ? "Expected a closing brace"
                         : std::format("Expected {} closing braces", ctx.block_depth);
        ctx.add_diagnostic(lsp::error(end.range, std::move(message)));
    }
}

void ki::par::handle_bad_token(Context& ctx, lex::Token const& token)
{
    auto message
        = std::format("Expected a definition, but found {}", lex::token_description(token.type));
    ctx.add_diagnostic(lsp::error(token.range, std::move(message)));
    skip_to_next_recovery_point(ctx);
    throw Failure {};
}
