#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>

auto libdesugar::Context::desugar(cst::Function_argument const& argument) -> ast::Function_argument
{
    return {
        .expression    = desugar(argument.expression),
        .argument_name = argument.name.transform([](auto const& syntax) { return syntax.name; }),
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
                if (auto const* const wildcard = std::get_if<cst::Wildcard>(&argument.value)) {
                    diagnostics().error(
                        source,
                        wildcard->source_range,
                        "A function default argument may not be a wildcard");
                }
                return desugar(std::get<utl::Wrapper<cst::Expression>>(argument.value));
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
    return {
        .value = std::visit<ast::Template_argument::Variant>(desugar(), argument.value),
    };
}

auto libdesugar::Context::desugar(cst::Template_parameter const& template_parameter)
    -> ast::Template_parameter
{
    return {
        .value = std::visit<ast::Template_parameter::Variant>(
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
            template_parameter.value),
        .source_range = template_parameter.source_range,
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
                    [](std::monostate) -> ast::Root_qualifier { return {}; },
                    [](cst::Root_qualifier::Global) -> ast::Root_qualifier {
                        return { .value = ast::Root_qualifier::Global {} };
                    },
                    [this](utl::Wrapper<cst::Type> const type) -> ast::Root_qualifier {
                        return { .value = desugar(type) };
                    },
                },
                root.value);
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
    return ast::Function_signature {
        .function_parameters
        = desugar(signature.function_parameters.value.normal_parameters.elements),
        .self_parameter = signature.function_parameters.value.self_parameter.transform(desugar()),
        .return_type    = signature.return_type.transform(desugar()),
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

auto libdesugar::Context::desugar(cst::Mutability const& mutability) -> ast::Mutability
{
    return ast::Mutability {
        .value = std::visit<ast::Mutability::Variant>(
            utl::Overload {
                [](cst::Mutability::Concrete const concrete) {
                    return ast::Mutability::Concrete { .is_mutable = concrete.is_mutable };
                },
                [](cst::Mutability::Parameterized const parameterized) {
                    return ast::Mutability::Parameterized { .name = parameterized.name };
                },
            },
            mutability.value),
        .is_explicit  = true,
        .source_range = mutability.source_range,
    };
}

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
        .value        = ast::Mutability::Concrete { .is_mutable = false },
        .is_explicit  = false,
        .source_range = source_range,
    };
}

auto libdesugar::Context::normalize_self_parameter(cst::Self_parameter const& self_parameter)
    -> ast::Function_parameter
{
    ast::Type self_type {
        .value        = ast::type::Self {},
        .source_range = self_parameter.source_range,
    };
    if (self_parameter.is_reference()) {
        self_type = ast::Type {
            .value = ast::type::Reference {
                .referenced_type = wrap(std::move(self_type)),
                .mutability      = desugar_mutability(self_parameter.mutability, self_parameter.self_keyword_token.source_range),
            },
            .source_range = self_parameter.source_range,
        };
    }
    ast::Pattern self_pattern {
        .value = ast::pattern::Name {
            .name {
                .identifier  = self_variable_identifier,
                .source_range = self_parameter.source_range,
            },
            .mutability = desugar_mutability(
                self_parameter.is_reference()
                    ? std::nullopt
                    : self_parameter.mutability,
                self_parameter.source_range),
        },
        .source_range = self_parameter.source_range,
    };
    return ast::Function_parameter {
        .pattern = wrap(std::move(self_pattern)),
        .type    = wrap(std::move(self_type)),
    };
}

auto libdesugar::Context::unit_value(utl::Source_range const view) -> utl::Wrapper<ast::Expression>
{
    return wrap(ast::Expression { .value = ast::expression::Tuple {}, .source_range = view });
}

auto libdesugar::Context::wildcard_pattern(utl::Source_range const view)
    -> utl::Wrapper<ast::Pattern>
{
    return wrap(ast::Pattern {
        .value        = ast::Wildcard { .source_range = view },
        .source_range = view,
    });
}

auto libdesugar::Context::true_pattern(utl::Source_range const view) -> utl::Wrapper<ast::Pattern>
{
    return wrap(ast::Pattern { .value = kieli::Boolean { true }, .source_range = view });
}

auto libdesugar::Context::false_pattern(utl::Source_range const view) -> utl::Wrapper<ast::Pattern>
{
    return wrap(ast::Pattern { .value = kieli::Boolean { false }, .source_range = view });
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
