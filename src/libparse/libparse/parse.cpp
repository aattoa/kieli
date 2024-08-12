#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libparse/parser_internals.hpp>

using namespace libparse;

namespace {
    auto parse_self_parameter(Context& context) -> std::optional<cst::Self_parameter>
    {
        auto const stage           = context.stage();
        auto const ampersand_token = context.try_extract(Token_type::ampersand);
        auto       mutability      = parse_mutability(context);
        if (auto self_token = context.try_extract(Token_type::lower_self)) {
            return cst::Self_parameter {
                .mutability         = mutability,
                .ampersand_token    = ampersand_token.transform(cst::Token::from_lexical),
                .self_keyword_token = cst::Token::from_lexical(self_token.value()),
            };
        }
        context.unstage(stage);
        return std::nullopt;
    }

    template <class Default, parser auto parse_argument>
    auto parse_default_argument(Context& context) -> std::optional<Default>
    {
        return context.try_extract(Token_type::equals).transform([&](Token const& equals) {
            if (auto const wildcard = context.try_extract(Token_type::underscore)) {
                return Default {
                    .variant           = cst::Wildcard { .range = wildcard.value().range },
                    .equals_sign_token = cst::Token::from_lexical(equals),
                };
            }
            return Default {
                .variant           = require<parse_argument>(context, "a default argument"),
                .equals_sign_token = cst::Token::from_lexical(equals),
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
        if (auto const colon = context.try_extract(Token_type::colon)) {
            if (auto const mut_keyword = context.try_extract(Token_type::mut)) {
                return cst::Template_mutability_parameter {
                    .name              = name,
                    .colon_token       = cst::Token::from_lexical(colon.value()),
                    .mut_keyword_token = cst::Token::from_lexical(mut_keyword.value()),
                    .default_argument  = parse_mutability_parameter_default_argument(context),
                };
            }
            if (auto const type = parse_type(context)) {
                return cst::Template_value_parameter {
                    .name = name,
                    .type_annotation { cst::Type_annotation {
                        .type        = type.value(),
                        .colon_token = cst::Token::from_lexical(colon.value()),
                    } },
                    .default_argument = parse_value_parameter_default_argument(context),
                };
            }
            context.error_expected("'mut' or a type");
        }
        return cst::Template_value_parameter {
            .name             = name,
            .default_argument = parse_value_parameter_default_argument(context),
        };
    }

    auto extract_template_type_parameter(Context& context, kieli::Upper const name)
        -> cst::Template_parameter_variant
    {
        if (auto const colon = context.try_extract(Token_type::colon)) {
            return cst::Template_type_parameter {
                .name             = name,
                .colon_token      = cst::Token::from_lexical(colon.value()),
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
        return cst::Import {
            .segments             = require<parse_segments>(context, "a module path"),
            .import_keyword_token = cst::Token::from_lexical(import_keyword),
        };
    }
} // namespace

auto libparse::parse_mutability(Context& context) -> std::optional<cst::Mutability>
{
    if (auto mut_keyword = context.try_extract(Token_type::mut)) {
        if (auto question_mark = context.try_extract(Token_type::question)) {
            return cst::Mutability {
                .variant = cst::mutability::Parameterized {
                    .name = extract_lower_name(context, "a mutability parameter name"),
                    .question_mark_token = cst::Token::from_lexical(question_mark.value()),
                },
                .range = context.up_to_current(mut_keyword.value().range),
                .mut_or_immut_keyword_token = cst::Token::from_lexical(mut_keyword.value()),
            };
        }
        return cst::Mutability {
            .variant                    = cst::mutability::Concrete::mut,
            .range                      = mut_keyword.value().range,
            .mut_or_immut_keyword_token = cst::Token::from_lexical(mut_keyword.value()),
        };
    }
    if (auto immut_keyword = context.try_extract(Token_type::immut)) {
        return cst::Mutability {
            .variant                    = cst::mutability::Concrete::immut,
            .range                      = immut_keyword.value().range,
            .mut_or_immut_keyword_token = cst::Token::from_lexical(immut_keyword.value()),
        };
    }
    return std::nullopt;
}

auto libparse::parse_concept_reference(Context& context) -> std::optional<cst::Concept_reference>
{
    // TODO: cleanup

    auto const anchor_range = context.peek().range;

    auto path = std::invoke([&]() -> std::optional<cst::Path> {
        std::optional<cst::Path_root> root;
        if (context.peek().type != Token_type::upper_name
            && context.peek().type != Token_type::lower_name) {
            if (auto const global = context.try_extract(Token_type::global)) {
                root = cst::Path_root {
                    .variant = cst::Path_root_global {
                        .global_keyword = cst::Token::from_lexical(global.value()),
                    },
                    .double_colon_token
                    = cst::Token::from_lexical(context.require_extract(Token_type::double_colon)),
                    .range = global.value().range,
                };
            }
            else {
                return std::nullopt;
            }
        }

        auto path = extract_path(context, std::move(root));
        if (!path.head.is_upper()) {
            context.error_expected(path.head.range, "a concept name");
        }
        return path;
    });

    if (path.has_value()) {
        auto template_arguments = parse_template_arguments(context);
        return cst::Concept_reference {
            .template_arguments = std::move(template_arguments),
            .path               = std::move(path.value()),
            .range              = context.up_to_current(anchor_range),
        };
    }
    return std::nullopt;
}

auto libparse::parse_type_annotation(Context& context) -> std::optional<cst::Type_annotation>
{
    return context.try_extract(Token_type::colon).transform([&](Token const& token) {
        return cst::Type_annotation {
            .type        = require<parse_type>(context, "a type"),
            .colon_token = cst::Token::from_lexical(token),
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
    if (auto const wildcard = context.try_extract(Token_type::underscore)) {
        return cst::Template_argument { cst::Wildcard { .range = wildcard->range } };
    }
    if (auto const type = parse_type(context)) {
        return cst::Template_argument { type.value() };
    }
    if (auto const expression = parse_expression(context)) {
        return cst::Template_argument { expression.value() };
    }
    if (auto const immut_keyword = context.try_extract(Token_type::immut)) {
        return cst::Template_argument { cst::Mutability {
            .variant                    = cst::mutability::Concrete::immut,
            .range                      = immut_keyword->range,
            .mut_or_immut_keyword_token = cst::Token::from_lexical(immut_keyword.value()),
        } };
    }
    return parse_mutability(context).transform([](cst::Mutability&& mutability) {
        return cst::Template_argument { std::move(mutability) };
    });
}

auto libparse::parse_template_arguments(Context& context) -> std::optional<cst::Template_arguments>
{
    return parse_bracketed<
        pretend_parse<
            extract_comma_separated_zero_or_more<parse_template_argument, "a template argument">>,
        "">(context);
}

auto libparse::parse_function_parameters(Context& context)
    -> std::optional<cst::Function_parameters>
{
    static constexpr auto extract_normals
        = extract_comma_separated_zero_or_more<parse_function_parameter, "a function parameter">;

    auto self_parameter         = parse_self_parameter(context);
    auto comma_token_after_self = context.try_extract(Token_type::comma);
    auto normal_parameters      = extract_normals(context);

    if (!self_parameter.has_value() && comma_token_after_self.has_value()) {
        context.error_expected(
            comma_token_after_self.value().range, "a function parameter before the comma");
    }
    if (comma_token_after_self.has_value() && normal_parameters.elements.empty()) {
        context.error_expected("a function parameter");
    }
    if (self_parameter.has_value() && !comma_token_after_self.has_value()
        && !normal_parameters.elements.empty()) {
        context.error_expected(
            context.cst().patterns[normal_parameters.elements.front().pattern].range,
            "a comma separating the self parameter from the other function parameters");
    }

    return cst::Function_parameters {
        .normal_parameters      = std::move(normal_parameters),
        .self_parameter         = std::move(self_parameter),
        .comma_token_after_self = comma_token_after_self.transform(cst::Token::from_lexical),
    };
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

auto libparse::parse_function_argument(Context& context) -> std::optional<cst::Function_argument>
{
    Stage const stage = context.stage();
    if (auto const name = parse_lower_name(context)) {
        if (auto equals_sign = context.try_extract(Token_type::equals)) {
            return cst::Function_argument {
                .name { cst::Name_lower_equals {
                    .name              = name.value(),
                    .equals_sign_token = cst::Token::from_lexical(equals_sign.value()),
                } },
                .expression = require<parse_expression>(context, "expression"),
            };
        }
        context.unstage(stage);
    }
    return parse_expression(context).transform([](cst::Expression_id const expression) {
        return cst::Function_argument { .expression = expression };
    });
}

auto libparse::parse_function_arguments(Context& context) -> std::optional<cst::Function_arguments>
{
    return parse_parenthesized<
        pretend_parse<
            extract_comma_separated_zero_or_more<parse_function_argument, "a function argument">>,
        "">(context);
}

auto libparse::extract_path(Context& context, std::optional<cst::Path_root>&& root) -> cst::Path
{
    auto const anchor_range = context.peek().range;

    cst::Separated_sequence<cst::Path_segment> segments;
    for (;;) {
        Token const token = context.extract();
        if (token.type == Token_type::upper_name || token.type == Token_type::lower_name) {
            kieli::Name const segment_name {
                .identifier = token.value_as<kieli::Identifier>(),
                .range      = token.range,
            };
            auto const template_arguments_stage = context.stage();
            auto       template_arguments       = parse_template_arguments(context);

            if (auto const double_colon = context.try_extract(Token_type::double_colon)) {
                segments.separator_tokens.push_back(cst::Token::from_lexical(double_colon.value()));
                segments.elements.push_back({
                    .template_arguments          = std::move(template_arguments),
                    .name                        = segment_name,
                    .trailing_double_colon_token = cst::Token::from_lexical(double_colon.value()),
                });
                continue;
            }
            // Primary name encountered
            context.unstage(template_arguments_stage);

            kieli::Range const range
                = context.up_to_current(root.has_value() ? root.value().range : anchor_range);

            return cst::Path {
                .segments = std::move(segments),
                .root     = std::move(root),
                .head     = segment_name,
                .range    = range,
            };
        }
        context.error_expected(token.range, "an identifier");
    }
}

auto libparse::extract_concept_references(Context& context)
    -> cst::Separated_sequence<cst::Concept_reference>
{
    return require<
        parse_separated_one_or_more<parse_concept_reference, "a concept name", Token_type::plus>>(
        context, "one or more '+'-separated concept names");
}

auto kieli::parse(Database& db, Document_id const document_id) -> CST
{
    db.documents.at(document_id).diagnostics.clear();

    auto arena   = cst::Arena {};
    auto context = libparse::Context { arena, lex_state(db, document_id) };

    std::vector<cst::Import> imports;
    while (auto const import_token = context.try_extract(Token_type::import_)) {
        imports.push_back(extract_import(context, import_token.value()));
    }

    std::vector<cst::Definition> definitions;
    while (auto definition = libparse::parse_definition(context)) {
        definitions.push_back(std::move(definition.value()));
    }
    if (!context.is_finished()) {
        context.error_expected("a definition");
    }

    return CST { CST::Module {
        .imports     = std::move(imports),
        .definitions = std::move(definitions),
        .arena       = std::move(arena),
        .source      = document_id,
    } };
}
