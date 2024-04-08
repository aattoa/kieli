#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>

auto libdesugar::Context::desugar(cst::Function_argument const& argument) -> ast::Function_argument
{
    return {
        .expression = desugar(argument.expression),
        .name       = argument.name.transform([](auto const& syntax) { return syntax.name; }),
    };
}

auto libdesugar::Context::desugar(cst::Function_parameter const& parameter)
    -> ast::Function_parameter
{
    return ast::Function_parameter {
        .pattern          = desugar(parameter.pattern),
        .type             = parameter.type.transform(wrap_desugar()),
        .default_argument = parameter.default_argument.transform(
            [&](cst::Value_parameter_default_argument const& argument) {
                if (auto const* const wildcard = std::get_if<cst::Wildcard>(&argument.variant)) {
                    diagnostics().error(
                        source,
                        wildcard->source_range,
                        "A function default argument may not be a wildcard");
                }
                return desugar(std::get<utl::Wrapper<cst::Expression>>(argument.variant));
            }),
    };
}

auto libdesugar::Context::desugar(cst::Wildcard const& wildcard) -> ast::Wildcard
{
    return ast::Wildcard { .source_range = wildcard.source_range };
}

auto libdesugar::Context::desugar(cst::Self_parameter const& self_parameter) -> ast::Self_parameter
{
    return ast::Self_parameter {
        .mutability = desugar_mutability(
            self_parameter.mutability, self_parameter.self_keyword_token.source_range),
        .is_reference = self_parameter.is_reference(),
        .source_range = self_parameter.source_range,
    };
}

auto libdesugar::Context::desugar(cst::Template_argument const& argument) -> ast::Template_argument
{
    return std::visit<ast::Template_argument>(desugar(), argument);
}

auto libdesugar::Context::desugar(cst::Template_parameter const& template_parameter)
    -> ast::Template_parameter
{
    return {
        std::visit<ast::Template_parameter::Variant>(
            utl::Overload {
                [&](cst::Template_type_parameter const& parameter) {
                    return ast::Template_type_parameter {
                        .name             = parameter.name,
                        .classes          = desugar(parameter.classes),
                        .default_argument = parameter.default_argument.transform(desugar()),
                    };
                },
                [&](cst::Template_value_parameter const& parameter) {
                    return ast::Template_value_parameter {
                        .name             = parameter.name,
                        .type             = parameter.type_annotation.transform(wrap_desugar()),
                        .default_argument = parameter.default_argument.transform(desugar()),
                    };
                },
                [&](cst::Template_mutability_parameter const& parameter) {
                    return ast::Template_mutability_parameter {
                        .name             = parameter.name,
                        .default_argument = parameter.default_argument.transform(desugar()),
                    };
                },
            },
            template_parameter.variant),
        template_parameter.source_range,
    };
}

auto libdesugar::Context::desugar(cst::Qualifier const& qualifier) -> ast::Qualifier
{
    return ast::Qualifier {
        .template_arguments = qualifier.template_arguments.transform(desugar()),
        .name               = qualifier.name,
        .source_range       = qualifier.source_range,
    };
}

auto libdesugar::Context::desugar(cst::Qualified_name const& name) -> ast::Qualified_name
{
    return ast::Qualified_name {
        .middle_qualifiers = desugar(name.middle_qualifiers.elements),
        .root_qualifier    = name.root_qualifier.transform([&](cst::Root_qualifier const& root) {
            return std::visit(
                utl::Overload {
                    [](cst::Global_root_qualifier const&) -> ast::Root_qualifier {
                        return ast::Global_root_qualifier {};
                    },
                    [this](utl::Wrapper<cst::Type> const type) -> ast::Root_qualifier {
                        return desugar(type);
                    },
                },
                root.variant);
        }),
        .primary_name = name.primary_name,
    };
}

auto libdesugar::Context::desugar(cst::Class_reference const& reference) -> ast::Class_reference
{
    return ast::Class_reference {
        .template_arguments = reference.template_arguments.transform(desugar()),
        .name               = desugar(reference.name),
        .source_range       = reference.source_range,
    };
}

auto libdesugar::Context::desugar(cst::Function_signature const& signature)
    -> ast::Function_signature
{
    std::vector<ast::Function_parameter> parameters;
    if (signature.function_parameters.value.self_parameter.has_value()) {
        parameters.push_back(
            normalize_self_parameter(signature.function_parameters.value.self_parameter.value()));
    }
    parameters.append_range(std::views::transform(
        signature.function_parameters.value.normal_parameters.elements, desugar()));

    ast::Type return_type = signature.return_type.has_value()
                              ? desugar(*signature.return_type.value().type)
                              : unit_type(signature.name.source_range);

    return ast::Function_signature {
        .template_parameters = signature.template_parameters.transform(desugar()),
        .function_parameters = std::move(parameters),
        .self_parameter = signature.function_parameters.value.self_parameter.transform(desugar()),
        .return_type    = std::move(return_type),
        .name           = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::Type_signature const& signature) -> ast::Type_signature
{
    return ast::Type_signature {
        .classes = desugar(signature.classes.elements),
        .name    = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::expression::Struct_initializer::Field const& field)
    -> ast::expression::Struct_initializer::Field
{
    return { .name = field.name, .expression = desugar(field.expression) };
}

auto libdesugar::Context::desugar(cst::Mutability const& mutability) -> ast::Mutability
{
    return ast::Mutability {
        .variant = std::visit<ast::Mutability::Variant>(
            utl::Overload {
                [](cst::mutability::Concrete const concrete) -> ast::mutability::Concrete {
                    return concrete;
                },
                [](cst::mutability::Parameterized const parameterized) {
                    return ast::mutability::Parameterized { parameterized.name };
                },
            },
            mutability.variant),
        .is_explicit  = true,
        .source_range = mutability.source_range,
    };
}

auto libdesugar::Context::desugar(cst::pattern::Field const& field) -> ast::pattern::Field
{
    return ast::pattern::Field {
        .name    = field.name,
        .pattern = field.field_pattern.transform(
            [&](cst::pattern::Field::Field_pattern const& field_pattern) {
                return desugar(field_pattern.pattern);
            }),
    };
}

auto libdesugar::Context::desugar(cst::pattern::Constructor_body const& body)
    -> ast::pattern::Constructor_body
{
    return std::visit<ast::pattern::Constructor_body>(
        utl::Overload {
            [&](cst::pattern::Struct_constructor const& constructor) {
                return ast::pattern::Struct_constructor { .fields = desugar(constructor.fields) };
            },
            [&](cst::pattern::Tuple_constructor const& constructor) {
                return ast::pattern::Tuple_constructor { .pattern = desugar(constructor.pattern) };
            },
            [&](cst::pattern::Unit_constructor const&) {
                return ast::pattern::Unit_constructor {};
            },
        },
        body);
};

auto libdesugar::Context::desugar(cst::Type_annotation const& annotation) -> ast::Type
{
    return desugar(*annotation.type);
}

auto libdesugar::Context::desugar_mutability(
    std::optional<cst::Mutability> const& mutability,
    utl::Source_range const               source_range) -> ast::Mutability
{
    if (mutability.has_value()) {
        return desugar(mutability.value());
    }
    return ast::Mutability {
        .variant      = ast::mutability::Concrete::immut,
        .is_explicit  = false,
        .source_range = source_range,
    };
}

auto libdesugar::Context::normalize_self_parameter(cst::Self_parameter const& self_parameter)
    -> ast::Function_parameter
{
    ast::Type self_type {
        .variant      = ast::type::Self {},
        .source_range = self_parameter.source_range,
    };
    if (self_parameter.is_reference()) {
        self_type = ast::Type {
            ast::type::Reference {
                .referenced_type = wrap(std::move(self_type)),
                .mutability      = desugar_mutability(
                    self_parameter.mutability, self_parameter.self_keyword_token.source_range),
            },
            self_parameter.source_range,
        };
    }
    return ast::Function_parameter {
        .pattern = wrap(ast::Pattern {
            ast::pattern::Name {
                .name {
                    .identifier   = self_variable_identifier,
                    .source_range = self_parameter.source_range,
                },
                .mutability = desugar_mutability(
                    self_parameter.is_reference() ? std::nullopt : self_parameter.mutability,
                    self_parameter.source_range),
            },
            self_parameter.source_range,
        }),
        .type    = wrap(std::move(self_type)),
    };
}

auto libdesugar::unit_type(utl::Source_range const source_range) -> ast::Type
{
    return ast::Type { ast::type::Tuple {}, source_range };
}

auto libdesugar::unit_value(utl::Source_range const source_range) -> ast::Expression
{
    return ast::Expression { ast::expression::Tuple {}, source_range };
}

auto libdesugar::wildcard_pattern(utl::Source_range const source_range) -> ast::Pattern
{
    return ast::Pattern { ast::Wildcard { source_range }, source_range };
}

auto libdesugar::Context::diagnostics() noexcept -> kieli::Diagnostics&
{
    return compile_info.diagnostics;
}

auto kieli::desugar(cst::Module const& module, Compile_info& compile_info) -> ast::Module
{
    auto                node_arena = ast::Node_arena::with_default_page_size();
    libdesugar::Context context { compile_info, node_arena, module.source };
    auto                definitions = context.desugar(module.definitions);
    return ast::Module {
        .definitions = std::move(definitions),
        .node_arena  = std::move(context.node_arena),
    };
}
