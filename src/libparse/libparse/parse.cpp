#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    template <typename Default, parser auto parse_argument>
    auto parse_default_argument(Context& context) -> std::optional<Default>
    {
        return context.try_extract(Token_type::equals).transform([&](Token const& equals) {
            context.add_semantic_token(equals.range, Semantic::operator_name);
            if (auto const underscore = context.try_extract(Token_type::underscore)) {
                return Default {
                    .variant           = cst::Wildcard { context.token(underscore.value()) },
                    .equals_sign_token = context.token(equals),
                };
            }
            return Default {
                .variant           = require<parse_argument>(context, "a default argument"),
                .equals_sign_token = context.token(equals),
            };
        });
    }

    constexpr auto parse_type_parameter_default_argument
        = parse_default_argument<cst::Type_parameter_default_argument, parse_type>;
    constexpr auto parse_value_parameter_default_argument
        = parse_default_argument<cst::Value_parameter_default_argument, parse_expression>;
    constexpr auto parse_mutability_parameter_default_argument
        = parse_default_argument<cst::Mutability_parameter_default_argument, parse_mutability>;

    auto extract_template_value_or_mutability_parameter(Context& context, kieli::Lower const name)
        -> cst::Template_parameter_variant
    {
        context.add_semantic_token(name.range, Semantic::parameter);
        Token const colon = context.require_extract(Token_type::colon);
        context.add_punctuation(colon);
        if (auto const mut_keyword = context.try_extract(Token_type::mut)) {
            context.add_keyword(mut_keyword.value());
            return cst::Template_mutability_parameter {
                .name              = name,
                .colon_token       = context.token(colon),
                .mut_keyword_token = context.token(mut_keyword.value()),
                .default_argument  = parse_mutability_parameter_default_argument(context),
            };
        }
        if (auto const type = parse_type(context)) {
            return cst::Template_value_parameter {
                .name = name,
                .type_annotation { cst::Type_annotation {
                    .type        = type.value(),
                    .colon_token = context.token(colon),
                } },
                .default_argument = parse_value_parameter_default_argument(context),
            };
        }
        context.error_expected("'mut' or a type");
    }

    auto extract_template_type_parameter(Context& context, kieli::Upper const name)
        -> cst::Template_parameter_variant
    {
        context.add_semantic_token(name.range, Semantic::type_parameter);
        if (auto const colon = context.try_extract(Token_type::colon)) {
            context.add_punctuation(colon.value());
            return cst::Template_type_parameter {
                .name             = name,
                .colon_token      = context.token(colon.value()),
                .concepts         = extract_concept_references(context),
                .default_argument = parse_type_parameter_default_argument(context),
            };
        }
        return cst::Template_type_parameter { .name = name };
    }

    auto dispatch_parse_template_parameter(Context& context)
        -> std::optional<cst::Template_parameter_variant>
    {
        auto const extract_lower
            = std::bind_front(extract_template_value_or_mutability_parameter, std::ref(context));
        auto const extract_upper
            = std::bind_front(extract_template_type_parameter, std::ref(context));
        return parse_lower_name(context).transform(extract_lower).or_else([&] {
            return parse_upper_name(context).transform(extract_upper);
        });
    }

    auto extract_import(Context& context, Token const& import_keyword) -> cst::Import
    {
        static constexpr parser auto parse_segments = parse_separated_one_or_more<
            parse_lower_name,
            "a module path segment",
            Token_type::dot>;
        context.add_keyword(import_keyword);
        return cst::Import {
            .segments             = require<parse_segments>(context, "a module path"),
            .import_keyword_token = context.token(import_keyword),
        };
    }

    auto parse_concept_path(Context& context) -> std::optional<cst::Path>
    {
        auto path = parse_simple_path(context);
        if (path.has_value()) {
            context.set_previous_path_head_semantic_type(Semantic::interface);
        }
        return path;
    }

    auto root_range(cst::Arena const& arena, cst::Path_root const& root)
        -> std::optional<kieli::Range>
    {
        if (auto const* const global = std::get_if<cst::Path_root_global>(&root)) {
            return arena.tokens[global->global_keyword].range;
        }
        if (auto const* const type_id = std::get_if<cst::Type_id>(&root)) {
            return arena.types[*type_id].range;
        }
        return std::nullopt;
    }
} // namespace

auto libparse::parse_simple_path_root(Context& context) -> std::optional<cst::Path_root>
{
    switch (context.peek().type) {
    case Token_type::lower_name:
    case Token_type::upper_name: return cst::Path_root {}; // Extract nothing
    case Token_type::global:     return cst::Path_root_global { context.token(context.extract()) };
    default:                     return std::nullopt;
    }
}

auto libparse::parse_simple_path(Context& context) -> std::optional<cst::Path>
{
    return parse_simple_path_root(context).transform(
        [&](cst::Path_root const root) { return extract_path(context, root); });
}

auto libparse::parse_complex_path(Context& context) -> std::optional<cst::Path>
{
    return parse_simple_path_root(context)
        .or_else([&] { return std::optional<cst::Path_root>(parse_type_root(context)); })
        .transform([&](cst::Path_root const root) { return extract_path(context, root); });
}

auto libparse::parse_mutability(Context& context) -> std::optional<cst::Mutability>
{
    if (auto mut_keyword = context.try_extract(Token_type::mut)) {
        context.add_keyword(mut_keyword.value());
        if (auto question_mark = context.try_extract(Token_type::question)) {
            context.add_semantic_token(question_mark.value().range, Semantic::operator_name);
            return cst::Mutability {
                .variant = cst::Parameterized_mutability {
                    .name = extract_lower_name(context, "a mutability parameter name"),
                    .question_mark_token = context.token(question_mark.value()),
                },
                .range = context.up_to_current(mut_keyword.value().range),
                .mut_or_immut_keyword_token = context.token(mut_keyword.value()),
            };
        }
        return cst::Mutability {
            .variant                    = kieli::Mutability::mut,
            .range                      = mut_keyword.value().range,
            .mut_or_immut_keyword_token = context.token(mut_keyword.value()),
        };
    }
    if (auto immut_keyword = context.try_extract(Token_type::immut)) {
        context.add_keyword(immut_keyword.value());
        return cst::Mutability {
            .variant                    = kieli::Mutability::immut,
            .range                      = immut_keyword.value().range,
            .mut_or_immut_keyword_token = context.token(immut_keyword.value()),
        };
    }
    return std::nullopt;
}

auto libparse::parse_type_annotation(Context& context) -> std::optional<cst::Type_annotation>
{
    return context.try_extract(Token_type::colon).transform([&](Token const& token) {
        context.add_punctuation(token);
        return cst::Type_annotation {
            .type        = require<parse_type>(context, "a type"),
            .colon_token = context.token(token),
        };
    });
}

auto libparse::parse_template_parameters(Context& context)
    -> std::optional<cst::Template_parameters>
{
    return parse_bracketed<
        parse_comma_separated_one_or_more<parse_template_parameter, "a template parameter">,
        "a bracketed list of template parameters">(context);
}

auto libparse::parse_template_parameter(Context& context) -> std::optional<cst::Template_parameter>
{
    auto const anchor_range = context.peek().range;
    return dispatch_parse_template_parameter(context).transform(
        [&](cst::Template_parameter_variant&& variant) {
            return cst::Template_parameter {
                .variant = std::move(variant),
                .range   = context.up_to_current(anchor_range),
            };
        });
}

auto libparse::parse_template_argument(Context& context) -> std::optional<cst::Template_argument>
{
    if (auto const underscore = context.try_extract(Token_type::underscore)) {
        context.add_semantic_token(underscore.value().range, Semantic::variable);
        return cst::Template_argument { cst::Wildcard { context.token(underscore.value()) } };
    }
    if (auto const type = parse_type(context)) {
        return cst::Template_argument { type.value() };
    }
    if (auto const expression = parse_expression(context)) {
        return cst::Template_argument { expression.value() };
    }
    if (auto const immut_keyword = context.try_extract(Token_type::immut)) {
        context.add_keyword(immut_keyword.value());
        return cst::Template_argument { cst::Mutability {
            .variant                    = kieli::Mutability::immut,
            .range                      = immut_keyword->range,
            .mut_or_immut_keyword_token = context.token(immut_keyword.value()),
        } };
    }
    return parse_mutability(context).transform([](cst::Mutability&& mutability) {
        return cst::Template_argument { std::move(mutability) };
    });
}

auto libparse::parse_template_arguments(Context& context) -> std::optional<cst::Template_arguments>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_template_argument, "a template argument">;
    return parse_bracketed<pretend_parse<extract>, "">(context);
}

auto libparse::parse_function_parameters(Context& context)
    -> std::optional<cst::Function_parameters>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_function_parameter, "a function parameter">;
    return parse_parenthesized<pretend_parse<extract>, "">(context);
}

auto libparse::parse_function_parameter(Context& context) -> std::optional<cst::Function_parameter>
{
    return parse_pattern(context).transform([&](cst::Pattern_id const pattern) {
        return cst::Function_parameter {
            .pattern          = pattern,
            .type             = parse_type_annotation(context),
            .default_argument = parse_value_parameter_default_argument(context),
        };
    });
}

auto libparse::parse_function_arguments(Context& context) -> std::optional<cst::Function_arguments>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_expression, "a function argument">;
    return parse_parenthesized<pretend_parse<extract>, "">(context);
}

auto libparse::extract_path(Context& context, cst::Path_root const root) -> cst::Path
{
    kieli::Range const             anchor_range = context.peek().range;
    std::vector<cst::Path_segment> segments;
    std::size_t                    head_semantic_token_offset {};

    auto extract_segment = [&](std::optional<cst::Token_id> const double_colon_token_id) {
        Token const token          = context.extract();
        head_semantic_token_offset = context.semantic_tokens().size();
        if (token.type == Token_type::upper_name) {
            context.add_semantic_token(token.range, Semantic::type);
        }
        else if (token.type == Token_type::lower_name) {
            context.add_semantic_token(token.range, Semantic::variable);
        }
        else {
            context.error_expected(token.range, "an identifier");
        }
        segments.push_back(cst::Path_segment {
            .template_arguments         = parse_template_arguments(context),
            .name                       = name_from_token(token),
            .leading_double_colon_token = double_colon_token_id,
        });
    };

    if (std::holds_alternative<std::monostate>(root)) {
        extract_segment(std::nullopt);
    }
    while (auto const double_colon = context.try_extract(Token_type::double_colon)) {
        context.add_semantic_token(double_colon.value().range, Semantic::operator_name);
        extract_segment(context.token(double_colon.value()));
    }
    if (segments.empty()) {
        context.error_expected("at least one path segment");
    }

    context.set_previous_path_head_semantic_token_offset(head_semantic_token_offset);
    if (context.peek().type == Token_type::paren_open) {
        context.set_previous_path_head_semantic_type(Semantic::function);
    }

    return cst::Path {
        .root     = root,
        .segments = std::move(segments),
        .range    = context.up_to_current(root_range(context.cst(), root).value_or(anchor_range)),
    };
}

auto libparse::extract_concept_references(Context& context) -> cst::Separated_sequence<cst::Path>
{
    static constexpr auto parse_paths
        = parse_separated_one_or_more<parse_concept_path, "a concept path", Token_type::plus>;
    return require<parse_paths>(context, "one or more '+'-separated concept paths");
}

auto kieli::parse(Database& db, Document_id const id) -> CST
{
    auto arena   = cst::Arena {};
    auto context = libparse::Context { arena, lex_state(db, id) };

    std::vector<cst::Import> imports;
    while (auto const import_token = context.try_extract(Token_type::import_)) {
        imports.push_back(extract_import(context, import_token.value()));
    }

    std::vector<cst::Definition> definitions;
    while (not context.is_finished()) {
        try {
            if (auto definition = libparse::parse_definition(context)) {
                definitions.push_back(std::move(definition).value());
            }
            else {
                kieli::add_error(db, id, context.peek().range, "Expected a definition");
                context.skip_to_next_recovery_point();
            }
        }
        catch (Parse_error const&) {
            context.skip_to_next_recovery_point();
        }
    }

    db.documents.at(id).semantic_tokens = std::move(context.semantic_tokens());

    return CST { CST::Module {
        .imports     = std::move(imports),
        .definitions = std::move(definitions),
        .arena       = std::move(arena),
        .document_id = id,
    } };
}
