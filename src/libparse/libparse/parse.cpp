#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libparse/internals.hpp>

using namespace ki::parse;

namespace {
    template <typename Default, std::invocable<Context&> auto parse_argument>
    auto parse_default_argument(Context& ctx) -> std::optional<Default>
    {
        return try_extract(ctx, lex::Type::Equals).transform([&](Token const& equals) {
            add_semantic_token(ctx, equals.range, Semantic::Operator_name);
            if (auto const underscore = try_extract(ctx, lex::Type::Underscore)) {
                return Default {
                    .variant           = cst::Wildcard { token(ctx, underscore.value()) },
                    .equals_sign_token = token(ctx, equals),
                };
            }
            return Default {
                .variant           = require<parse_argument>(ctx, "a default argument"),
                .equals_sign_token = token(ctx, equals),
            };
        });
    }

    constexpr auto parse_type_parameter_default_argument
        = parse_default_argument<cst::Type_parameter_default_argument, parse_type>;
    constexpr auto parse_value_parameter_default_argument
        = parse_default_argument<cst::Value_parameter_default_argument, parse_expression>;
    constexpr auto parse_mutability_parameter_default_argument
        = parse_default_argument<cst::Mutability_parameter_default_argument, parse_mutability>;

    auto extract_template_value_or_mutability_parameter(Context& ctx, ki::Lower const name)
        -> cst::Template_parameter_variant
    {
        add_semantic_token(ctx, name.range, Semantic::Parameter);
        Token const colon = require_extract(ctx, lex::Type::Colon);
        add_punctuation(ctx, colon.range);
        if (auto const mut_keyword = try_extract(ctx, lex::Type::Mut)) {
            add_keyword(ctx, mut_keyword.value().range);
            return cst::Template_mutability_parameter {
                .name             = name,
                .colon_token      = token(ctx, colon),
                .mut_token        = token(ctx, mut_keyword.value()),
                .default_argument = parse_mutability_parameter_default_argument(ctx),
            };
        }
        if (auto const type = parse_type(ctx)) {
            return cst::Template_value_parameter {
                .name = name,
                .type_annotation { cst::Type_annotation {
                    .type        = type.value(),
                    .colon_token = token(ctx, colon),
                } },
                .default_argument = parse_value_parameter_default_argument(ctx),
            };
        }
        error_expected(ctx, "'mut' or a type");
    }

    auto extract_template_type_parameter(Context& ctx, ki::Upper const name)
        -> cst::Template_parameter_variant
    {
        add_semantic_token(ctx, name.range, Semantic::Type_parameter);
        if (auto const colon = try_extract(ctx, lex::Type::Colon)) {
            add_punctuation(ctx, colon.value().range);
            return cst::Template_type_parameter {
                .name             = name,
                .colon_token      = token(ctx, colon.value()),
                .concepts         = extract_concept_references(ctx),
                .default_argument = parse_type_parameter_default_argument(ctx),
            };
        }
        return cst::Template_type_parameter {
            .name             = name,
            .colon_token      = {},
            .concepts         = {},
            .default_argument = {},
        };
    }

    auto dispatch_parse_template_parameter(Context& ctx)
        -> std::optional<cst::Template_parameter_variant>
    {
        auto const extract_lower
            = std::bind_front(extract_template_value_or_mutability_parameter, std::ref(ctx));
        auto const extract_upper = std::bind_front(extract_template_type_parameter, std::ref(ctx));
        return parse_lower_name(ctx).transform(extract_lower).or_else([&] {
            return parse_upper_name(ctx).transform(extract_upper);
        });
    }

    auto extract_import(Context& ctx, Token const& import_keyword) -> cst::Import
    {
        static constexpr auto parse_segments = parse_separated_one_or_more<
            parse_lower_name,
            "a module path segment",
            lex::Type::Dot>;
        add_keyword(ctx, import_keyword.range);
        return cst::Import {
            .segments     = require<parse_segments>(ctx, "a module path"),
            .import_token = token(ctx, import_keyword),
        };
    }

    auto parse_concept_path(Context& ctx) -> std::optional<cst::Path>
    {
        auto path = parse_simple_path(ctx);
        if (path.has_value()) {
            set_previous_path_head_semantic_type(ctx, Semantic::Interface);
        }
        return path;
    }

    auto root_range(cst::Arena const& arena, cst::Path_root const& root) -> std::optional<ki::Range>
    {
        if (auto const* const global = std::get_if<cst::Path_root_global>(&root)) {
            return arena.range[global->global_token];
        }
        if (auto const* const type_id = std::get_if<cst::Type_id>(&root)) {
            return arena.type[*type_id].range;
        }
        return std::nullopt;
    }
} // namespace

auto ki::parse::parse_simple_path_root(Context& ctx) -> std::optional<cst::Path_root>
{
    switch (peek(ctx).type) {
    case lex::Type::Lower_name:
    case lex::Type::Upper_name: return cst::Path_root {}; // Extract nothing
    case lex::Type::Global:     return cst::Path_root_global { token(ctx, extract(ctx)) };
    default:                    return std::nullopt;
    }
}

auto ki::parse::parse_simple_path(Context& ctx) -> std::optional<cst::Path>
{
    return parse_simple_path_root(ctx).transform(
        [&](cst::Path_root const root) { return extract_path(ctx, root); });
}

auto ki::parse::parse_complex_path(Context& ctx) -> std::optional<cst::Path>
{
    return parse_simple_path_root(ctx)
        .or_else([&] { return std::optional<cst::Path_root>(parse_type_root(ctx)); })
        .transform([&](cst::Path_root const root) { return extract_path(ctx, root); });
}

auto ki::parse::parse_mutability(Context& ctx) -> std::optional<cst::Mutability>
{
    if (auto mut_keyword = try_extract(ctx, lex::Type::Mut)) {
        add_keyword(ctx, mut_keyword.value().range);
        if (auto question_mark = try_extract(ctx, lex::Type::Question)) {
            add_semantic_token(ctx, question_mark.value().range, Semantic::Operator_name);
            return cst::Mutability {
                .variant = cst::Parameterized_mutability {
                    .name = extract_lower_name(ctx, "a mutability parameter name"),
                    .question_mark_token = token(ctx,question_mark.value()),
                },
                .range = up_to_current(ctx,mut_keyword.value().range),
                .mut_or_immut_token = token(ctx,mut_keyword.value()),
            };
        }
        return cst::Mutability {
            .variant            = Mutability::Mut,
            .range              = mut_keyword.value().range,
            .mut_or_immut_token = token(ctx, mut_keyword.value()),
        };
    }
    if (auto immut_keyword = try_extract(ctx, lex::Type::Immut)) {
        add_keyword(ctx, immut_keyword.value().range);
        return cst::Mutability {
            .variant            = Mutability::Immut,
            .range              = immut_keyword.value().range,
            .mut_or_immut_token = token(ctx, immut_keyword.value()),
        };
    }
    return std::nullopt;
}

auto ki::parse::parse_type_annotation(Context& ctx) -> std::optional<cst::Type_annotation>
{
    return try_extract(ctx, lex::Type::Colon).transform([&](Token const& colon) {
        add_punctuation(ctx, colon.range);
        return cst::Type_annotation {
            .type        = require<parse_type>(ctx, "a type"),
            .colon_token = token(ctx, colon),
        };
    });
}

auto ki::parse::parse_template_parameters(Context& ctx) -> std::optional<cst::Template_parameters>
{
    return parse_bracketed<
        parse_comma_separated_one_or_more<parse_template_parameter, "a template parameter">,
        "a bracketed list of template parameters">(ctx);
}

auto ki::parse::parse_template_parameter(Context& ctx) -> std::optional<cst::Template_parameter>
{
    auto const anchor_range = peek(ctx).range;
    return dispatch_parse_template_parameter(ctx).transform(
        [&](cst::Template_parameter_variant&& variant) {
            return cst::Template_parameter {
                .variant = std::move(variant),
                .range   = up_to_current(ctx, anchor_range),
            };
        });
}

auto ki::parse::parse_template_argument(Context& ctx) -> std::optional<cst::Template_argument>
{
    if (auto const underscore = try_extract(ctx, lex::Type::Underscore)) {
        add_semantic_token(ctx, underscore.value().range, Semantic::Variable);
        return cst::Template_argument { cst::Wildcard { token(ctx, underscore.value()) } };
    }
    if (auto const type = parse_type(ctx)) {
        return cst::Template_argument { type.value() };
    }
    if (auto const expression = parse_expression(ctx)) {
        return cst::Template_argument { expression.value() };
    }
    if (auto const immut_keyword = try_extract(ctx, lex::Type::Immut)) {
        add_keyword(ctx, immut_keyword.value().range);
        return cst::Template_argument { cst::Mutability {
            .variant            = Mutability::Immut,
            .range              = immut_keyword->range,
            .mut_or_immut_token = token(ctx, immut_keyword.value()),
        } };
    }
    return parse_mutability(ctx).transform(utl::make<cst::Template_argument>);
}

auto ki::parse::parse_template_arguments(Context& ctx) -> std::optional<cst::Template_arguments>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_template_argument, "a template argument">;
    return parse_bracketed<pretend_parse<extract>, "">(ctx);
}

auto ki::parse::parse_function_parameters(Context& ctx) -> std::optional<cst::Function_parameters>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_function_parameter, "a function parameter">;
    return parse_parenthesized<pretend_parse<extract>, "">(ctx);
}

auto ki::parse::parse_function_parameter(Context& ctx) -> std::optional<cst::Function_parameter>
{
    return parse_pattern(ctx).transform([&](cst::Pattern_id const pattern) {
        return cst::Function_parameter {
            .pattern          = pattern,
            .type             = parse_type_annotation(ctx),
            .default_argument = parse_value_parameter_default_argument(ctx),
        };
    });
}

auto ki::parse::parse_function_arguments(Context& ctx) -> std::optional<cst::Function_arguments>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_expression, "a function argument">;
    return parse_parenthesized<pretend_parse<extract>, "">(ctx);
}

auto ki::parse::extract_path(Context& ctx, cst::Path_root const root) -> cst::Path
{
    auto anchor_range               = peek(ctx).range;
    auto segments                   = std::vector<cst::Path_segment> {};
    auto head_semantic_token_offset = 0UZ;

    auto extract_segment = [&](std::optional<cst::Range_id> const double_colon_token_id) {
        Token const token          = extract(ctx);
        head_semantic_token_offset = ctx.semantic_tokens.size();
        if (token.type == lex::Type::Upper_name) {
            add_semantic_token(ctx, token.range, Semantic::Type);
        }
        else if (token.type == lex::Type::Lower_name) {
            add_semantic_token(ctx, token.range, Semantic::Variable);
        }
        else {
            error_expected(ctx, token.range, "an identifier");
        }
        segments.push_back(cst::Path_segment {
            .template_arguments         = parse_template_arguments(ctx),
            .name                       = name(ctx, token),
            .leading_double_colon_token = double_colon_token_id,
        });
    };

    if (std::holds_alternative<std::monostate>(root)) {
        extract_segment(std::nullopt);
    }
    while (auto const double_colon = try_extract(ctx, lex::Type::Double_colon)) {
        add_semantic_token(ctx, double_colon.value().range, Semantic::Operator_name);
        extract_segment(token(ctx, double_colon.value()));
    }
    if (segments.empty()) {
        error_expected(ctx, "at least one path segment");
    }

    ctx.previous_path_semantic_offset = head_semantic_token_offset;
    if (peek(ctx).type == lex::Type::Paren_open) {
        set_previous_path_head_semantic_type(ctx, Semantic::Function);
    }

    return cst::Path {
        .root     = root,
        .segments = std::move(segments),
        .range    = up_to_current(ctx, root_range(ctx.arena, root).value_or(anchor_range)),
    };
}

auto ki::parse::extract_concept_references(Context& ctx) -> cst::Separated<cst::Path>
{
    return require<
        parse_separated_one_or_more<parse_concept_path, "a concept path", lex::Type::Plus>>(
        ctx, "one or more '+'-separated concept paths");
}

auto ki::parse::parse(Database& db, Document_id const id) -> cst::Module
{
    auto arena = cst::Arena {};
    auto ctx   = context(db, arena, id);

    std::vector<cst::Import> imports;
    while (auto const import_token = try_extract(ctx, lex::Type::Import)) {
        imports.push_back(extract_import(ctx, import_token.value()));
    }

    std::vector<cst::Definition> definitions;
    while (not is_finished(ctx)) {
        try {
            if (auto definition = parse_definition(ctx)) {
                definitions.push_back(std::move(definition).value());
            }
            else {
                add_error(db, id, peek(ctx).range, "Expected a definition");
                skip_to_next_recovery_point(ctx);
            }
        }
        catch (Failure const&) {
            skip_to_next_recovery_point(ctx);
        }
    }

    db.documents[id].semantic_tokens = std::move(ctx.semantic_tokens);

    return cst::Module {
        .imports     = std::move(imports),
        .definitions = std::move(definitions),
        .arena       = std::move(arena),
        .doc_id      = id,
    };
}
