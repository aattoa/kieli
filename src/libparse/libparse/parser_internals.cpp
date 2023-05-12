#include <libutl/common/utilities.hpp>
#include <libparse/parser_internals.hpp>


namespace {

    auto parse_template_argument(Parse_context& context)
        -> tl::optional<ast::Template_argument>
    {
        if (Token const* const wildcard = context.try_extract(Token::Type::underscore)) {
            return ast::Template_argument {
                ast::Template_argument::Wildcard { .source_view = wildcard->source_view }
            };
        }
        if (auto type = parse_type(context)) {
            return ast::Template_argument { context.wrap(std::move(*type)) };
        }
        else if (auto expression = parse_expression(context)) {
            return ast::Template_argument { context.wrap(std::move(*expression)) };
        }
        else {
            tl::optional<ast::Mutability> mutability;

            if (Token const* const token = context.try_extract(Token::Type::immut)) {
                mutability = ast::Mutability {
                    .value       = ast::Mutability::Concrete { .is_mutable = false },
                    .source_view = token->source_view
                };
            }
            else {
                ast::Mutability const mut = extract_mutability(context);
                if (mut.was_explicitly_specified())
                    mutability = mut;
            }

            if (mutability.has_value())
                return ast::Template_argument { std::move(*mutability) };
            return tl::nullopt;
        }
    }

    auto parse_template_parameter(Parse_context& context)
        -> tl::optional<ast::Template_parameter>
    {
        auto const template_parameter = [&, anchor = context.pointer](ast::Name const name, ast::Template_parameter::Variant&& value) {
            tl::optional<ast::Template_argument> default_argument;

            if (context.try_consume(Token::Type::equals))
                default_argument = extract_required<parse_template_argument, "a default template argument">(context);

            return ast::Template_parameter {
                .value            = std::move(value),
                .name             = name,
                .default_argument = std::move(default_argument),
                .source_view      = make_source_view(anchor, context.pointer - 1)
            };
        };

        if (auto name = parse_lower_name(context)) {
            if (context.try_consume(Token::Type::colon)) {
                if (context.try_consume(Token::Type::mut))
                    return template_parameter(*name, ast::Template_parameter::Mutability_parameter {});
                else if (auto type = parse_type(context))
                    return template_parameter(*name, ast::Template_parameter::Value_parameter { .type = context.wrap(std::move(*type)) });
                else
                    context.error_expected("'mut' or a type");
            }

            return template_parameter(*name, ast::Template_parameter::Value_parameter { .type = tl::nullopt });
        }
        else if (auto name = parse_upper_name(context)) {
            std::vector<ast::Class_reference> classes;
            if (context.try_consume(Token::Type::colon))
                classes = extract_class_references(context);
            return template_parameter(*name, ast::Template_parameter::Type_parameter { .classes = std::move(classes) });
        }
        else {
            return tl::nullopt;
        }
    }

    auto ensure_correct_template_parameters(
        Parse_context                                & context,
        std::span<ast::Template_parameter const> const parameters) -> void
    {
        ast::Template_parameter const* last_defaulted_parameter = nullptr;

        for (ast::Template_parameter const& parameter : parameters) {
            if (parameter.default_argument.has_value()) {
                last_defaulted_parameter = &parameter;

                ast::Template_argument const& argument = utl::get(parameter.default_argument);

                auto const emit_parameter_argument_mismatch_error = [&] {
                    std::string_view const parameter_description = utl::match(
                        parameter.value,
                        [](ast::Template_parameter::Type_parameter       const&) { return "type"; },
                        [](ast::Template_parameter::Mutability_parameter const&) { return "mutability"; },
                        [](ast::Template_parameter::Value_parameter      const&) { return "value"; });
                    std::string_view const argument_description = utl::match(argument.value,
                        [](utl::Wrapper<ast::Type>          const&) { return "type"; },
                        [](ast::Mutability                  const&) { return "mutability"; },
                        [](utl::Wrapper<ast::Expression>    const&) { return "value"; },
                        [](ast::Template_argument::Wildcard const&) { return "wildcard"; });
                    context.error(parameter.source_view,
                        { "Invalid default template argument: {} parameter's default argument is a {} argument"_format(parameter_description, argument_description) });
                };

                std::visit(utl::Overload {
                    [](ast::Template_parameter::Type_parameter       const&, utl::Wrapper<ast::Type>           const&) {},
                    [](ast::Template_parameter::Mutability_parameter const&, ast::Mutability                  const&) {},
                    [](ast::Template_parameter::Value_parameter      const&, utl::Wrapper<ast::Expression>     const&) {},
                    [](auto                                          const&, ast::Template_argument::Wildcard const&) {},
                    [&](auto const&, auto const&) {
                        // Offloaded to the lambda to avoid template instantiation bloat
                        emit_parameter_argument_mismatch_error();
                    }
                }, parameter.value, argument.value);
            }
            else if (last_defaulted_parameter != nullptr) {
                context.error(last_defaulted_parameter->source_view, {
                    "Template parameters with default arguments must appear at the end of the parameter list"
                });
            }
        }
    }

}


auto parse_top_level_pattern(Parse_context& context) -> tl::optional<ast::Pattern> {
    return parse_comma_separated_one_or_more<parse_pattern, "a pattern">(context)
        .transform([](std::vector<ast::Pattern>&& patterns) -> ast::Pattern
    {
        if (patterns.size() == 1)
            return std::move(patterns.front());
        auto const source_view =
            patterns.front().source_view.combine_with(patterns.back().source_view);
        return ast::Pattern {
            .value       = ast::pattern::Tuple { std::move(patterns) },
            .source_view = source_view,
        };
    });
}


auto parse_template_arguments(Parse_context& context)
    -> tl::optional<std::vector<ast::Template_argument>>
{
    static constexpr auto extract_arguments =
        extract_comma_separated_zero_or_more<parse_template_argument, "a template argument">;

    if (context.try_consume(Token::Type::bracket_open)) {
        auto arguments = extract_arguments(context);
        context.consume_required(Token::Type::bracket_close);
        return arguments;
    }
    return tl::nullopt;
}


auto parse_template_parameters(Parse_context& context)
    -> tl::optional<std::vector<ast::Template_parameter>>
{
    static constexpr auto extract_parameters =
        parse_comma_separated_one_or_more<parse_template_parameter, "a template parameter">;

    if (context.try_consume(Token::Type::bracket_open)) {
        if (auto parameters = extract_parameters(context)) {
            context.consume_required(Token::Type::bracket_close);
            ensure_correct_template_parameters(context, *parameters);
            return parameters;
        }
        context.error_expected("one or more template parameters");
    }
    return tl::nullopt;
}


auto extract_function_parameters(Parse_context& context)
    -> std::vector<ast::Function_parameter>
{
    return extract_comma_separated_zero_or_more<
        [](Parse_context& context)
            -> tl::optional<ast::Function_parameter>
        {
            if (auto pattern = parse_pattern(context)) {
                tl::optional<ast::Type> type;
                if (context.try_consume(Token::Type::colon))
                    type = extract_type(context);

                tl::optional<ast::Expression> default_value;
                if (context.try_consume(Token::Type::equals))
                    default_value = extract_expression(context);

                return ast::Function_parameter {
                    std::move(*pattern),
                    std::move(type),
                    std::move(default_value)
                };
            }
            return tl::nullopt;
        },
        "a function parameter"
    >(context);
}


auto extract_qualified(ast::Root_qualifier&& root, Parse_context& context)
    -> ast::Qualified_name
{
    std::vector<ast::Qualifier> qualifiers;
    Token* template_argument_anchor = nullptr;

    auto extract_qualifier = [&]() -> bool {
        switch (auto& token = context.extract(); token.type) {
        case Token::Type::lower_name:
        case Token::Type::upper_name:
        {
            template_argument_anchor = context.pointer;
            auto template_arguments = parse_template_arguments(context);

            ast::Qualifier qualifier {
                .template_arguments = std::move(template_arguments),
                .name {
                    .identifier  = token.as_identifier(),
                    .is_upper    = token.type == Token::Type::upper_name,
                    .source_view = template_argument_anchor[-1].source_view
                },
                .source_view = make_source_view(template_argument_anchor - 1, context.pointer - 1)
            };

            qualifiers.push_back(std::move(qualifier));
            return true;
        }
        default:
            context.retreat();
            return false;
        }
    };

    if (extract_qualifier()) {
        while (context.try_consume(Token::Type::double_colon)) {
            if (!extract_qualifier())
                context.error_expected("an identifier");
        }

        auto back = std::move(qualifiers.back());
        qualifiers.pop_back();

        // Ignore potential template arguments, they are handled separately
        context.pointer = template_argument_anchor;

        return {
            .middle_qualifiers = std::move(qualifiers),
            .root_qualifier    = std::move(root),
            .primary_name      = back.name,
        };
    }
    else {
        // root:: followed by no qualifiers
        context.error_expected("an identifier");
    }
}


auto extract_mutability(Parse_context& context) -> ast::Mutability {
    auto const get_source_view = [&, anchor = context.pointer] {
        return context.pointer == anchor
            ? anchor->source_view
            : make_source_view(anchor, context.pointer - 1);
    };

    if (context.try_consume(Token::Type::mut)) {
        if (context.try_consume(Token::Type::question)) {
            compiler::Identifier const parameter = extract_lower_id(context, "a mutability parameter name");
            return {
                .value       = ast::Mutability::Parameterized { .identifier = parameter },
                .source_view = get_source_view()
            };
        }
        else {
            return {
                .value       = ast::Mutability::Concrete { .is_mutable = true },
                .source_view = get_source_view()
            };
        }
    }
    else if (context.try_consume(Token::Type::immut)) {
        context.error(get_source_view(), { "Immutability may not be specified explicitly, as it is the default" });
    }
    else {
        return {
            .value       = ast::Mutability::Concrete { .is_mutable = false },
            .source_view = get_source_view()
        };
    }
}


auto parse_class_reference(Parse_context& context)
    -> tl::optional<ast::Class_reference>
{
    auto* const anchor = context.pointer;

    auto name = std::invoke([&]() -> tl::optional<ast::Qualified_name> {
        ast::Root_qualifier root;
        Token const* const anchor = context.pointer;

        if (context.try_consume(Token::Type::upper_name) || context.try_consume(Token::Type::lower_name))
            context.retreat();
        else if (context.try_consume(Token::Type::double_colon))
            root.value = ast::Root_qualifier::Global{};
        else
            return tl::nullopt;

        auto name = extract_qualified(std::move(root), context);

        if (name.primary_name.is_upper.get())
            return name;

        context.error(
            make_source_view(anchor, context.pointer),
            { "Expected a class name, but found a lowercase identifier" });
    });

    if (name.has_value()) {
        auto template_arguments = parse_template_arguments(context);

        return ast::Class_reference {
            .template_arguments = std::move(template_arguments),
            .name               = std::move(*name),
            .source_view        = make_source_view(anchor, context.pointer - 1)
        };
    }
    return tl::nullopt;
}


auto extract_class_references(Parse_context& context)
    -> std::vector<ast::Class_reference>
{
    static constexpr auto extract_classes =
        extract_separated_zero_or_more<parse_class_reference, Token::Type::plus, "a class name">;

    auto classes = extract_classes(context);

    if (classes.empty())
        context.error_expected("one or more class names");
    else
        return classes;
}


auto Parse_context::diagnostics() noexcept -> utl::diagnostics::Builder& {
    return compilation_info.get()->diagnostics;
}
auto Parse_context::error(utl::Source_view const erroneous_view, utl::diagnostics::Message_arguments const& arguments) -> void {
    compilation_info.get()->diagnostics.emit_error(arguments.add_source_view(erroneous_view));
}
auto Parse_context::error(utl::diagnostics::Message_arguments const& arguments) -> void {
    error(make_source_view(pointer, pointer + 1), arguments);
}
auto Parse_context::error_expected(utl::Source_view const erroneous_view, std::string_view const expectation, tl::optional<std::string_view> const help) -> void {
    error(erroneous_view, {
        .message = fmt::format(
            "Expected {}, but found {}",
            expectation,
            compiler::token_description(pointer->type)),
        .help_note = help,
    });
}
auto Parse_context::error_expected(std::string_view const expectation, tl::optional<std::string_view> const help) -> void {
    error_expected(pointer->source_view, expectation, help);
}
