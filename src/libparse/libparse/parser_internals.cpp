#include <libparse/parser_internals.hpp>
#include <libutl/common/utilities.hpp>

namespace {

    using namespace libparse;

    auto parse_template_argument(Context& context) -> std::optional<cst::Template_argument>
    {
        if (Lexical_token const* const wildcard = context.try_extract(Token_type::underscore)) {
            return cst::Template_argument { cst::Template_argument::Wildcard {
                .source_view = wildcard->source_view,
            } };
        }
        if (auto type = parse_type(context)) {
            return cst::Template_argument { std::move(type.value()) };
        }
        if (auto expression = parse_expression(context)) {
            return cst::Template_argument { expression.value() };
        }
        if (Lexical_token const* const immut_keyword = context.try_extract(Token_type::immut)) {
            return cst::Template_argument {
                cst::Mutability {
                    .value                      = cst::Mutability::Concrete { .is_mutable = false },
                    .source_view                = immut_keyword->source_view,
                    .mut_or_immut_keyword_token = cst::Token::from_lexical(immut_keyword),
                },
            };
        }
        return parse_mutability(context).transform([](cst::Mutability&& mutability) {
            return cst::Template_argument { std::move(mutability) };
        });
    }

    auto is_valid_template_parameter_default_argument(
        cst::Template_parameter::Variant const& parameter,
        cst::Template_argument::Variant const&  argument) -> bool
    {
        return std::holds_alternative<cst::Template_argument::Wildcard>(argument)
            || (std::holds_alternative<cst::Template_parameter::Type_parameter>(parameter)
                && std::holds_alternative<utl::Wrapper<cst::Type>>(argument))
            || (std::holds_alternative<cst::Template_parameter::Value_parameter>(parameter)
                && std::holds_alternative<utl::Wrapper<cst::Expression>>(argument))
            || (std::holds_alternative<cst::Template_parameter::Mutability_parameter>(parameter)
                && std::holds_alternative<cst::Mutability>(argument));
    }

    auto template_parameter_kind_description(cst::Template_parameter::Variant const& parameter)
        -> std::string_view
    {
        return std::visit(
            utl::Overload {
                [](cst::Template_parameter::Type_parameter const&) { return "type"; },
                [](cst::Template_parameter::Value_parameter const&) { return "value"; },
                [](cst::Template_parameter::Mutability_parameter const&) { return "mutability"; },
            },
            parameter);
    }

    auto template_argument_kind_description(cst::Template_argument::Variant const& argument)
        -> std::string_view
    {
        return std::visit(
            utl::Overload {
                [](cst::Template_argument::Wildcard const&) { return "wildcard"; },
                [](utl::Wrapper<cst::Type> const&) { return "type"; },
                [](utl::Wrapper<cst::Expression> const&) { return "value"; },
                [](cst::Mutability const&) { return "mutability"; },
            },
            argument);
    }

    auto parse_template_parameter_default_argument(
        cst::Template_parameter::Variant const& variant,
        Context& context) -> std::optional<cst::Template_parameter::Default_argument>
    {
        if (Lexical_token const* const equals_sign = context.try_extract(Token_type::equals)) {
            cst::Template_parameter::Default_argument default_argument {
                .argument
                = extract_required<parse_template_argument, "a default template argument">(context),
                .equals_sign_token = cst::Token::from_lexical(equals_sign),
            };
            if (!is_valid_template_parameter_default_argument(
                    variant, default_argument.argument.value))
            {
                context.diagnostics().error(
                    default_argument.argument.source_view(),
                    "A template {0} parameter's default argument must be a {0} argument, but found "
                    "a {1} argument",
                    template_parameter_kind_description(variant),
                    template_argument_kind_description(default_argument.argument.value));
            }
            return default_argument;
        }
        return std::nullopt;
    }

    auto extract_value_or_mutability_template_parameter(
        kieli::Name_lower const name,
        Lexical_token const*&   colon,
        Context&                context) -> cst::Template_parameter::Variant
    {
        colon = context.try_extract(Token_type::colon);
        if (colon) {
            if (Lexical_token const* const mut_keyword = context.try_extract(Token_type::mut)) {
                return cst::Template_parameter::Mutability_parameter {
                    .name              = name,
                    .mut_keyword_token = cst::Token::from_lexical(mut_keyword),
                };
            }
            if (auto type = parse_type(context)) {
                return cst::Template_parameter::Value_parameter {
                    .type = type.value(),
                    .name = name,
                };
            }
            context.error_expected("'mut' or a type");
        }
        return cst::Template_parameter::Value_parameter {
            .type = std::nullopt,
            .name = name,
        };
    }

    auto extract_type_template_parameter(
        kieli::Name_upper const name,
        Lexical_token const*&   colon,
        Context&                context) -> cst::Template_parameter::Variant
    {
        colon = context.try_extract(Token_type::colon);
        cst::Separated_sequence<cst::Class_reference> class_references;
        if (colon) {
            class_references = extract_class_references(context);
        }
        return cst::Template_parameter::Type_parameter {
            .classes = std::move(class_references),
            .name    = name,
        };
    }

    auto parse_template_parameter(Context& context) -> std::optional<cst::Template_parameter>
    {
        Lexical_token const* colon {};

        auto const make = [&, anchor = context.pointer](cst::Template_parameter::Variant&& value) {
            auto const source_view      = context.make_source_view(anchor, context.pointer - 1);
            auto       default_argument = parse_template_parameter_default_argument(value, context);
            return cst::Template_parameter {
                .value            = std::move(value),
                .colon_token      = optional_token(colon),
                .default_argument = std::move(default_argument),
                .source_view      = source_view,
            };
        };

        if (auto name = parse_lower_name(context)) {
            return make(
                extract_value_or_mutability_template_parameter(name.value(), colon, context));
        }
        if (auto name = parse_upper_name(context)) {
            return make(extract_type_template_parameter(name.value(), colon, context));
        }
        return std::nullopt;
    }

    auto parse_function_parameter(Context& context) -> std::optional<cst::Function_parameter>
    {
        if (auto pattern = parse_pattern(context)) {
            auto type_annotation = parse_type_annotation(context);
            std::optional<cst::Function_parameter::Default_argument> default_value;
            if (Lexical_token const* const equals_sign = context.try_extract(Token_type::equals)) {
                default_value = cst::Function_parameter::Default_argument {
                    .expression        = extract_expression(context),
                    .equals_sign_token = cst::Token::from_lexical(equals_sign),
                };
            }
            return cst::Function_parameter {
                .pattern          = *pattern,
                .type             = std::move(type_annotation),
                .default_argument = std::move(default_value),
            };
        }
        return std::nullopt;
    }

} // namespace

auto libparse::parse_top_level_pattern(Context& context)
    -> std::optional<utl::Wrapper<cst::Pattern>>
{
    return parse_comma_separated_one_or_more<parse_pattern, "a pattern">(context).transform(
        [&](cst::Separated_sequence<utl::Wrapper<cst::Pattern>>&& patterns)
            -> utl::Wrapper<cst::Pattern> {
            if (patterns.elements.size() == 1) {
                return patterns.elements.front();
            }
            auto const source_view = patterns.elements.front()->source_view.combine_with(
                patterns.elements.back()->source_view);
            return context.wrap(cst::Pattern {
                .value       = cst::pattern::Top_level_tuple { std::move(patterns) },
                .source_view = source_view,
            });
        });
}

auto libparse::parse_template_arguments(Context& context) -> std::optional<cst::Template_arguments>
{
    static constexpr auto extract_arguments
        = extract_comma_separated_zero_or_more<parse_template_argument, "a template argument">;
    Lexical_token const* const open = context.try_extract(Token_type::bracket_open);
    if (!open) {
        return std::nullopt;
    }
    auto                       arguments = extract_arguments(context);
    Lexical_token const* const close     = context.extract_required(Token_type::bracket_close);
    return cst::Template_arguments {
        .value       = std::move(arguments),
        .open_token  = cst::Token::from_lexical(open),
        .close_token = cst::Token::from_lexical(close),
    };
}

auto libparse::parse_template_parameters(Context& context)
    -> std::optional<cst::Template_parameters>
{
    static constexpr auto extract_parameters
        = parse_comma_separated_one_or_more<parse_template_parameter, "a template parameter">;
    if (Lexical_token const* const open = context.try_extract(Token_type::bracket_open)) {
        if (auto parameters = extract_parameters(context)) {
            Lexical_token const* const close = context.extract_required(Token_type::bracket_close);
            return cst::Template_parameters {
                .value       = std::move(*parameters),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            };
        }
        context.error_expected("one or more template parameters");
    }
    return std::nullopt;
}

auto libparse::extract_function_parameters(Context& context)
    -> cst::Separated_sequence<cst::Function_parameter>
{
    return extract_comma_separated_zero_or_more<parse_function_parameter, "a function parameter">(
        context);
}

auto libparse::extract_qualified(Context& context, std::optional<cst::Root_qualifier>&& root)
    -> cst::Qualified_name
{
    cst::Separated_sequence<cst::Qualifier> middle_qualifiers;
    for (;;) {
        Lexical_token const& token = context.extract();
        if (is_name_token_type(token.type)) {
            kieli::Name_dynamic const qualifier_name {
                .identifier  = token.value_as<kieli::Identifier>(),
                .source_view = token.source_view,
                .is_upper    = token.type == Token_type::upper_name,
            };
            Lexical_token const* const template_argument_anchor = context.pointer;
            auto                       template_arguments       = parse_template_arguments(context);
            Lexical_token const* const double_colon = context.try_extract(Token_type::double_colon);
            if (double_colon) {
                middle_qualifiers.separator_tokens.push_back(
                    cst::Token::from_lexical(double_colon));
                middle_qualifiers.elements.push_back({
                    .template_arguments          = std::move(template_arguments),
                    .name                        = qualifier_name,
                    .trailing_double_colon_token = cst::Token::from_lexical(double_colon),
                    .source_view                 = token.source_view,
                });
                continue;
            }
            // Primary name encountered
            context.pointer = template_argument_anchor;
            return cst::Qualified_name {
                .middle_qualifiers = std::move(middle_qualifiers),
                .root_qualifier    = std::move(root),
                .primary_name      = qualifier_name,
            };
        }
        context.retreat();
        context.error_expected("an identifier");
    }
}

auto libparse::parse_mutability(Context& context) -> std::optional<cst::Mutability>
{
    if (Lexical_token const* const mut_keyword = context.try_extract(Token_type::mut)) {
        if (Lexical_token const* const question_mark = context.try_extract(Token_type::question)) {
            auto parameter_name = extract_lower_name(context, "a mutability parameter name");
            return cst::Mutability {
                .value = cst::Mutability::Parameterized {
                    .name = std::move(parameter_name),
                    .question_mark_token = cst::Token::from_lexical(question_mark),
                },
                .source_view = context.make_source_view(mut_keyword, context.pointer - 1),
                .mut_or_immut_keyword_token = cst::Token::from_lexical(mut_keyword),
            };
        }
        return cst::Mutability {
            .value                      = cst::Mutability::Concrete { .is_mutable = true },
            .source_view                = mut_keyword->source_view,
            .mut_or_immut_keyword_token = cst::Token::from_lexical(mut_keyword),
        };
    }
    if (Lexical_token const* const immut_keyword = context.try_extract(Token_type::immut)) {
        context.diagnostics().error(
            immut_keyword->source_view,
            "Immutability may not be specified here, as it is the default");
    }
    return std::nullopt;
}

auto libparse::parse_type_annotation(libparse::Context& context)
    -> std::optional<cst::Type_annotation>
{
    if (Lexical_token const* const colon = context.try_extract(Token_type::colon)) {
        return cst::Type_annotation {
            .type        = extract_type(context),
            .colon_token = cst::Token::from_lexical(colon),
        };
    }
    return std::nullopt;
}

auto libparse::parse_class_reference(Context& context) -> std::optional<cst::Class_reference>
{
    Lexical_token const* const anchor = context.pointer;

    auto name = std::invoke([&]() -> std::optional<cst::Qualified_name> {
        std::optional<cst::Root_qualifier> root;
        Lexical_token const* const         anchor = context.pointer;

        if (context.try_consume(Token_type::upper_name)
            || context.try_consume(Token_type::lower_name))
        {
            context.retreat();
        }
        else if (
            Lexical_token const* const double_colon = context.try_extract(Token_type::double_colon))
        {
            root = cst::Root_qualifier {
                .value              = cst::Root_qualifier::Global {},
                .double_colon_token = cst::Token::from_lexical(double_colon),
            };
        }
        else {
            return std::nullopt;
        }

        auto name = extract_qualified(context, std::move(root));
        if (name.primary_name.is_upper.get()) {
            return name;
        }

        context.diagnostics().error(
            context.make_source_view(anchor, context.pointer),
            "Expected a class name, but found a lowercase identifier");
    });

    if (name.has_value()) {
        auto template_arguments = parse_template_arguments(context);
        return cst::Class_reference {
            .template_arguments = std::move(template_arguments),
            .name               = std::move(*name),
            .source_view        = context.make_source_view(anchor, context.pointer - 1),
        };
    }
    return std::nullopt;
}

auto libparse::extract_class_references(Context& context)
    -> cst::Separated_sequence<cst::Class_reference>
{
    auto classes
        = parse_separated_one_or_more<parse_class_reference, Token_type::plus, "a class name">(
            context);
    if (classes.has_value()) {
        return std::move(*classes);
    }
    context.error_expected("one or more '+'-separated class names");
}

auto libparse::is_name_token_type(Token_type const type) noexcept -> bool
{
    return type == Token_type::upper_name || type == Token_type::lower_name;
}

auto libparse::optional_token(kieli::Lexical_token const* const token) -> std::optional<cst::Token>
{
    if (token) {
        return std::optional<cst::Token> { cst::Token::from_lexical(token) };
    }
    return std::nullopt;
}

libparse::Context::Context(
    std::span<Lexical_token const> const tokens,
    cst::Node_arena&                     node_arena,
    kieli::Compile_info&                 compile_info) noexcept
    : compile_info { compile_info }
    , node_arena { node_arena }
    , begin { tokens.data() }
    , end { begin + tokens.size() }
    , pointer { begin }
    , plus_id { compile_info.operator_pool.make("+") }
    , asterisk_id { compile_info.operator_pool.make("*") }
{
    // The end-of-input token should always be present
    utl::always_assert(!tokens.empty() && tokens.back().type == Token_type::end_of_input);
}

auto libparse::Context::is_finished() const noexcept -> bool
{
    return pointer->type == Token_type::end_of_input;
}

auto libparse::Context::try_extract(Token_type const type) noexcept -> Lexical_token const*
{
    return pointer->type == type ? pointer++ : nullptr;
}

auto libparse::Context::extract() noexcept -> Lexical_token const&
{
    return *pointer++;
}

auto libparse::Context::previous() const noexcept -> Lexical_token const&
{
    assert(pointer && pointer != begin);
    return pointer[-1];
}

auto libparse::Context::extract_required(Token_type const type) -> Lexical_token const*
{
    if (pointer->type == type) {
        return pointer++;
    }
    error_expected("'{}'"_format(type));
}

auto libparse::Context::consume_required(Token_type const type) -> void
{
    (void)extract_required(type);
}

auto libparse::Context::try_consume(Token_type const type) noexcept -> bool
{
    bool const eq = pointer->type == type;
    if (eq) {
        ++pointer;
    }
    return eq;
}

auto libparse::Context::retreat() noexcept -> void
{
    --pointer;
}

auto libparse::Context::error_expected(
    utl::Source_view const erroneous_view, std::string_view const expectation) -> void
{
    diagnostics().error(
        erroneous_view,
        "Expected {}, but found {}",
        expectation,
        kieli::Lexical_token::description(pointer->type));
}

auto libparse::Context::error_expected(std::string_view const expectation) -> void
{
    error_expected(pointer->source_view, expectation);
}

auto libparse::Context::diagnostics() noexcept -> kieli::Diagnostics&
{
    return compile_info.diagnostics;
}

auto libparse::Context::make_source_view(
    Lexical_token const* const first,
    Lexical_token const* const last) const noexcept -> utl::Source_view
{
    assert(first && last);
    assert(std::less_equal()(first, last));
    assert(std::less()(last, end));
    return first->source_view.combine_with(last->source_view);
}
