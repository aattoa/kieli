#include <libutl/common/utilities.hpp>
#include <libparse/parse.hpp>
#include <libparse/parser_internals.hpp>

namespace {

    using namespace libparse;

    auto parse_self_parameter(Context& context) -> std::optional<cst::Self_parameter>
    {
        Stage const stage           = context.stage();
        auto const  ampersand_token = context.try_extract(Token::Type::ampersand);
        auto        mutability      = parse_mutability(context);
        if (auto self_token = context.try_extract(Token::Type::lower_self)) {
            return cst::Self_parameter {
                .mutability         = mutability,
                .ampersand_token    = ampersand_token.transform(cst::Token::from_lexical),
                .self_keyword_token = cst::Token::from_lexical(self_token.value()),
                .source_range       = self_token.value().source_range,
            };
        }

        context.unstage(stage);
        return std::nullopt;
    }

    template <class Default, parser auto parse_argument>
    auto parse_default_argument(Context& context) -> std::optional<Default>
    {
        return context.try_extract(Token::Type::equals).transform([&](Token const& equals) {
            if (auto const wildcard = context.try_extract(Token::Type::underscore)) {
                return Default {
                    .equals_sign_token = cst::Token::from_lexical(equals),
                    .variant = cst::Wildcard { .source_range = wildcard.value().source_range },
                };
            }
            return Default {
                .equals_sign_token = cst::Token::from_lexical(equals),
                .variant           = require<parse_argument>(context, "a default argument"),
            };
        });
    }

    constexpr auto parse_type_parameter_default_argument
        = parse_default_argument<cst::Type_parameter_default_argument, parse_type>;
    constexpr auto parse_value_parameter_default_argument
        = parse_default_argument<cst::Value_parameter_default_argument, parse_expression>;
    constexpr auto parse_mutability_parameter_default_argument
        = parse_default_argument<cst::Mutability_parameter_default_argument, parse_mutability>;

    auto extract_template_value_or_mutability_parameter(
        Context& context, kieli::Name_lower const name) -> cst::Template_parameter::Variant
    {
        if (auto const colon = context.try_extract(Token::Type::colon)) {
            if (auto const mut_keyword = context.try_extract(Token::Type::mut)) {
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

    auto extract_template_type_parameter(Context& context, kieli::Name_upper const name)
        -> cst::Template_parameter::Variant
    {
        if (auto const colon = context.try_extract(Token::Type::colon)) {
            return cst::Template_type_parameter {
                .name             = name,
                .colon_token      = cst::Token::from_lexical(colon.value()),
                .classes          = extract_class_references(context),
                .default_argument = parse_type_parameter_default_argument(context),
            };
        }
        return cst::Template_type_parameter { .name = name };
    }

    auto dispatch_parse_template_parameter(Context& context)
        -> std::optional<cst::Template_parameter::Variant>
    {
        auto const extract_lower
            = std::bind_front(extract_template_value_or_mutability_parameter, std::ref(context));
        auto const extract_upper
            = std::bind_front(extract_template_type_parameter, std::ref(context));
        return parse_lower_name(context).transform(extract_lower).or_else([&] {
            return parse_upper_name(context).transform(extract_upper);
        });
    }

    auto extract_import(Context& context, Token const& import_keyword) -> cst::Module::Import
    {
        static constexpr parser auto parse_segments = parse_separated_one_or_more<
            parse_lower_name,
            "a module path segment",
            Token::Type::dot>;
        return cst::Module::Import {
            .segments             = require<parse_segments>(context, "a module path"),
            .import_keyword_token = cst::Token::from_lexical(import_keyword),
        };
    }

} // namespace

auto libparse::parse_mutability(Context& context) -> std::optional<cst::Mutability>
{
    if (auto mut_keyword = context.try_extract(Token::Type::mut)) {
        if (auto question_mark = context.try_extract(Token::Type::question)) {
            return cst::Mutability {
                .variant = cst::mutability::Parameterized {
                    .name = extract_lower_name(context, "a mutability parameter name"),
                    .question_mark_token = cst::Token::from_lexical(question_mark.value()),
                },
                .source_range = context.up_to_current(mut_keyword.value().source_range),
                .mut_or_immut_keyword_token = cst::Token::from_lexical(mut_keyword.value()),
            };
        }
        return cst::Mutability {
            .variant                    = cst::mutability::Concrete::mut,
            .source_range               = mut_keyword.value().source_range,
            .mut_or_immut_keyword_token = cst::Token::from_lexical(mut_keyword.value()),
        };
    }
    if (auto immut_keyword = context.try_extract(Token::Type::immut)) {
        context.compile_info().diagnostics.error(
            context.source(),
            immut_keyword.value().source_range,
            "Immutability may not be specified here, as it is the default");
    }
    return std::nullopt;
}

auto libparse::parse_class_reference(Context& context) -> std::optional<cst::Class_reference>
{
    // TODO: cleanup

    auto const anchor_source_range = context.peek().source_range;

    auto name = std::invoke([&]() -> std::optional<cst::Qualified_name> {
        std::optional<cst::Root_qualifier> root;
        if (context.peek().type != Token::Type::upper_name
            && context.peek().type != Token::Type::lower_name)
        {
            if (auto const global = context.try_extract(Token::Type::global)) {
                root = cst::Root_qualifier {
                    .variant = cst::Global_root_qualifier {
                        .global_keyword = cst::Token::from_lexical(global.value()),
                    },
                    .double_colon_token
                    = cst::Token::from_lexical(context.require_extract(Token::Type::double_colon)),
                    .source_range = global.value().source_range,
                };
            }
            else {
                return std::nullopt;
            }
        }

        auto name = extract_qualified_name(context, std::move(root));
        if (!name.primary_name.is_upper.get()) {
            context.compile_info().diagnostics.error(
                context.source(),
                name.primary_name.source_range,
                "Expected a class name, but found a lowercase identifier");
        }
        return name;
    });

    if (name.has_value()) {
        auto template_arguments = parse_template_arguments(context);
        return cst::Class_reference {
            .template_arguments = std::move(template_arguments),
            .name               = std::move(name.value()),
            .source_range       = context.up_to_current(anchor_source_range),
        };
    }
    return std::nullopt;
}

auto libparse::parse_type_annotation(Context& context) -> std::optional<cst::Type_annotation>
{
    return context.try_extract(Token::Type::colon).transform([&](Token const& token) {
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
    auto const anchor_source_range = context.peek().source_range;
    return dispatch_parse_template_parameter(context).transform(
        [&](cst::Template_parameter::Variant&& variant) {
            return cst::Template_parameter {
                .variant      = std::move(variant),
                .source_range = context.up_to_current(anchor_source_range),
            };
        });
}

auto libparse::parse_template_argument(Context& context) -> std::optional<cst::Template_argument>
{
    if (auto const wildcard = context.try_extract(Token::Type::underscore)) {
        return cst::Template_argument { cst::Wildcard { .source_range = wildcard->source_range } };
    }
    if (auto const type = parse_type(context)) {
        return cst::Template_argument { type.value() };
    }
    if (auto const expression = parse_expression(context)) {
        return cst::Template_argument { expression.value() };
    }
    if (auto const immut_keyword = context.try_extract(Token::Type::immut)) {
        return cst::Template_argument { cst::Mutability {
            .variant                    = cst::mutability::Concrete::immut,
            .source_range               = immut_keyword->source_range,
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
    auto comma_token_after_self = context.try_extract(Token::Type::comma);
    auto normal_parameters      = extract_normals(context);

    if (!self_parameter.has_value() && comma_token_after_self.has_value()) {
        context.error_expected(
            comma_token_after_self.value().source_range, "a function parameter before the comma");
    }
    if (comma_token_after_self.has_value() && normal_parameters.elements.empty()) {
        context.error_expected("a function parameter");
    }
    if (self_parameter.has_value() && !comma_token_after_self.has_value()
        && !normal_parameters.elements.empty())
    {
        context.error_expected(
            normal_parameters.elements.front().pattern->source_range,
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
    return parse_pattern(context).transform([&](utl::Wrapper<cst::Pattern> const pattern) {
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
        if (auto equals_sign = context.try_extract(Token::Type::equals)) {
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
    return parse_expression(context).transform([](utl::Wrapper<cst::Expression> const expression) {
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

auto libparse::extract_qualified_name(Context& context, std::optional<cst::Root_qualifier>&& root)
    -> cst::Qualified_name
{
    auto const anchor_source_range = context.peek().source_range;

    cst::Separated_sequence<cst::Qualifier> middle_qualifiers;
    for (;;) {
        Token const token = context.extract();
        if (token.type == Token::Type::upper_name || token.type == Token::Type::lower_name) {
            kieli::Name_dynamic const qualifier_name {
                .identifier   = token.value_as<kieli::Identifier>(),
                .source_range = token.source_range,
                .is_upper     = token.type == Token::Type::upper_name,
            };
            Stage const template_arguments_stage = context.stage();
            auto        template_arguments       = parse_template_arguments(context);

            if (auto const double_colon = context.try_extract(Token::Type::double_colon)) {
                middle_qualifiers.separator_tokens.push_back(
                    cst::Token::from_lexical(double_colon.value()));
                middle_qualifiers.elements.push_back({
                    .template_arguments          = std::move(template_arguments),
                    .name                        = qualifier_name,
                    .trailing_double_colon_token = cst::Token::from_lexical(double_colon.value()),
                    .source_range                = token.source_range,
                });
                continue;
            }
            // Primary name encountered
            context.unstage(template_arguments_stage);

            auto const source_range = context.up_to_current(
                root.has_value() ? root.value().source_range : anchor_source_range);

            return cst::Qualified_name {
                .middle_qualifiers = std::move(middle_qualifiers),
                .root_qualifier    = std::move(root),
                .primary_name      = qualifier_name,
                .source_range      = source_range,
            };
        }
        context.error_expected(token.source_range, "an identifier");
    }
}

auto libparse::extract_class_references(Context& context)
    -> cst::Separated_sequence<cst::Class_reference>
{
    return require<
        parse_separated_one_or_more<parse_class_reference, "a class reference", Token::Type::plus>>(
        context, "one or more '+'-separated class references");
}

auto kieli::parse(utl::Source::Wrapper const source, Compile_info& compile_info) -> cst::Module
{
    cst::Node_arena   node_arena = cst::Node_arena::with_default_page_size();
    libparse::Context context { node_arena, Lex_state::make(source, compile_info) };

    std::vector<cst::Module::Import> imports;
    while (auto const import_token = context.try_extract(Token::Type::import_)) {
        imports.push_back(extract_import(context, import_token.value()));
    }

    std::vector<cst::Definition> definitions;
    while (auto definition = libparse::parse_definition(context)) {
        definitions.push_back(std::move(definition.value()));
    }
    if (!context.is_finished()) {
        context.error_expected("a definition");
    }

    return cst::Module {
        .imports     = std::move(imports),
        .definitions = std::move(definitions),
        .node_arena  = std::move(node_arena),
        .source      = source,
    };
}
