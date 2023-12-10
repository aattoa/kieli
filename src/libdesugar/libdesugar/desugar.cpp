#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>

auto libdesugar::Context::desugar(cst::Function_argument const& argument) -> ast::Function_argument
{
    return {
        .expression = desugar(argument.expression),
        .argument_name
        = argument.argument_name.transform([](auto const& syntax) { return syntax.name; }),
    };
}

auto libdesugar::Context::desugar(cst::Function_parameter const& parameter)
    -> ast::Function_parameter
{
    return ast::Function_parameter {
        .pattern          = desugar(parameter.pattern),
        .type             = parameter.type.transform(utl::compose(wrap(), desugar())),
        .default_argument = parameter.default_argument.transform(desugar()),
    };
}

auto libdesugar::Context::desugar(cst::Self_parameter const& self_parameter) -> ast::Self_parameter
{
    return ast::Self_parameter {
        .mutability = desugar_mutability(
            self_parameter.mutability, self_parameter.self_keyword_token.source_view),
        .is_reference = self_parameter.is_reference(),
        .source_view  = self_parameter.source_view,
    };
}

auto libdesugar::Context::desugar(cst::Template_argument const& argument) -> ast::Template_argument
{
    return {
        .value = utl::match(
            argument.value,
            [this](auto const& argument) -> ast::Template_argument::Variant {
                return this->desugar(argument);
            },
            [](cst::Template_argument::Wildcard const wildcard) -> ast::Template_argument::Variant {
                return ast::Template_argument::Wildcard { .source_view = wildcard.source_view };
            }),
    };
}

auto libdesugar::Context::desugar(cst::Template_parameter const& parameter)
    -> ast::Template_parameter
{
    return {
        .value = utl::match(
            parameter.value,
            [this](cst::Template_parameter::Type_parameter const& type_parameter)
                -> ast::Template_parameter::Variant {
                return ast::Template_parameter::Type_parameter {
                    .classes = desugar(type_parameter.classes),
                    .name    = type_parameter.name,
                };
            },
            [this](cst::Template_parameter::Value_parameter const& value_parameter)
                -> ast::Template_parameter::Variant {
                return ast::Template_parameter::Value_parameter {
                    .type = value_parameter.type.transform(desugar()),
                    .name = value_parameter.name,
                };
            },
            [](cst::Template_parameter::Mutability_parameter const& mutability_parameter)
                -> ast::Template_parameter::Variant {
                return ast::Template_parameter::Mutability_parameter {
                    .name = mutability_parameter.name,
                };
            }),
        .default_argument = parameter.default_argument.transform(desugar()),
        .source_view      = parameter.source_view,
    };
}

auto libdesugar::Context::desugar(cst::Qualifier const& qualifier) -> ast::Qualifier
{
    return {
        .template_arguments = qualifier.template_arguments.transform(desugar()),
        .name               = qualifier.name,
        .source_view        = qualifier.source_view,
    };
}

auto libdesugar::Context::desugar(cst::Qualified_name const& name) -> ast::Qualified_name
{
    return {
        .middle_qualifiers = desugar(name.middle_qualifiers.elements),
        .root_qualifier    = name.root_qualifier.transform([&](cst::Root_qualifier const& root) {
            return utl::match(
                root.value,
                [](std::monostate) -> ast::Root_qualifier { return {}; },
                [](cst::Root_qualifier::Global) -> ast::Root_qualifier {
                    return { .value = ast::Root_qualifier::Global {} };
                },
                [this](utl::Wrapper<cst::Type> const type) -> ast::Root_qualifier {
                    return { .value = desugar(type) };
                });
        }),
        .primary_name = name.primary_name,
    };
}

auto libdesugar::Context::desugar(cst::Class_reference const& reference) -> ast::Class_reference
{
    return {
        .template_arguments = reference.template_arguments.transform(desugar()),
        .name               = desugar(reference.name),
        .source_view        = reference.source_view,
    };
}

auto libdesugar::Context::desugar(cst::Function_signature const& signature)
    -> ast::Function_signature
{
    return {
        .function_parameters = desugar(signature.function_parameters.normal_parameters.elements),
        .self_parameter      = signature.function_parameters.self_parameter.transform(desugar()),
        .return_type         = signature.return_type.transform(desugar()),
        .name                = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::Type_signature const& signature) -> ast::Type_signature
{
    return {
        .classes = desugar(signature.classes.elements),
        .name    = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::Mutability const& mutability) -> ast::Mutability
{
    return ast::Mutability {
        .value = utl::match(
            mutability.value,
            [](cst::Mutability::Concrete const concrete) -> ast::Mutability::Variant {
                return ast::Mutability::Concrete { .is_mutable = concrete.is_mutable };
            },
            [](cst::Mutability::Parameterized const parameterized) -> ast::Mutability::Variant {
                return ast::Mutability::Parameterized { .name = parameterized.name };
            }),
        .is_explicit = true,
        .source_view = mutability.source_view,
    };
}

auto libdesugar::Context::desugar(cst::Type_annotation const& annotation) -> ast::Type
{
    return desugar(*annotation.type);
}

auto libdesugar::Context::desugar(cst::Function_parameter::Default_argument const& default_argument)
    -> utl::Wrapper<ast::Expression>
{
    return desugar(default_argument.expression);
}

auto libdesugar::Context::desugar(cst::Template_parameter::Default_argument const& default_argument)
    -> ast::Template_argument
{
    return desugar(default_argument.argument);
}

auto libdesugar::Context::desugar_mutability(
    std::optional<cst::Mutability> const& mutability,
    utl::Source_view const                source_view) -> ast::Mutability
{
    if (mutability.has_value()) {
        return desugar(*mutability);
    }
    return ast::Mutability {
        .value       = ast::Mutability::Concrete { .is_mutable = false },
        .is_explicit = false,
        .source_view = source_view,
    };
}

auto libdesugar::Context::normalize_self_parameter(cst::Self_parameter const& self_parameter)
    -> ast::Function_parameter
{
    ast::Type self_type {
        .value       = ast::type::Self {},
        .source_view = self_parameter.source_view,
    };
    if (self_parameter.is_reference()) {
        self_type = ast::Type {
            .value = ast::type::Reference {
                .referenced_type = wrap(std::move(self_type)),
                .mutability      = desugar_mutability(self_parameter.mutability, self_parameter.self_keyword_token.source_view),
            },
            .source_view = self_parameter.source_view,
        };
    }
    ast::Pattern self_pattern {
        .value = ast::pattern::Name {
            .name {
                .identifier  = self_variable_identifier,
                .source_view = self_parameter.source_view,
            },
            .mutability = desugar_mutability(
                self_parameter.is_reference()
                    ? std::nullopt
                    : self_parameter.mutability,
                self_parameter.source_view),
        },
        .source_view = self_parameter.source_view,
    };
    return ast::Function_parameter {
        .pattern = wrap(std::move(self_pattern)),
        .type    = wrap(std::move(self_type)),
    };
}

auto libdesugar::Context::unit_value(utl::Source_view const view) -> utl::Wrapper<ast::Expression>
{
    return wrap(ast::Expression { .value = ast::expression::Tuple {}, .source_view = view });
}

auto libdesugar::Context::wildcard_pattern(utl::Source_view const view)
    -> utl::Wrapper<ast::Pattern>
{
    return wrap(ast::Pattern { .value = ast::pattern::Wildcard {}, .source_view = view });
}

auto libdesugar::Context::true_pattern(utl::Source_view const view) -> utl::Wrapper<ast::Pattern>
{
    return wrap(ast::Pattern { .value = kieli::Boolean { true }, .source_view = view });
}

auto libdesugar::Context::false_pattern(utl::Source_view const view) -> utl::Wrapper<ast::Pattern>
{
    return wrap(ast::Pattern { .value = kieli::Boolean { false }, .source_view = view });
}

auto libdesugar::Context::diagnostics() noexcept -> kieli::Diagnostics&
{
    return compile_info.diagnostics;
}

auto kieli::desugar(cst::Module const& module, Compile_info& compile_info) -> ast::Module
{
    auto                node_arena = ast::Node_arena::with_default_page_size();
    libdesugar::Context context { compile_info, node_arena };
    auto                definitions = context.desugar(module.definitions);
    return ast::Module {
        .definitions = std::move(definitions),
        .node_arena  = std::move(context.node_arena),
    };
}
