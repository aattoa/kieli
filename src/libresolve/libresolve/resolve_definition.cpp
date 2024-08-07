#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto resolve_function_parameter(
        Context&                       context,
        Inference_state&               state,
        Scope&                         scope,
        hir::Environment_id const      environment,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        cpputil::always_assert(parameter.type.has_value()); // TODO

        hir::Pattern pattern = resolve_pattern(
            context, state, scope, environment, context.ast.patterns[parameter.pattern]);
        hir::Type const type = resolve_type(
            context, state, scope, environment, context.ast.types[parameter.type.value()]);

        require_subtype_relationship(
            context, state, context.hir.types[pattern.type], context.hir.types[type.id]);

        auto default_argument = parameter.default_argument.transform(
            [&](ast::Expression_id const argument) {
                hir::Expression expression = resolve_expression(
                    context, state, scope, environment, context.ast.expressions[argument]);
                require_subtype_relationship(
                    context, state, context.hir.types[expression.type], context.hir.types[type.id]);
                return expression;
            });

        return hir::Function_parameter {
            .pattern          = std::move(pattern),
            .type             = type,
            .default_argument = std::move(default_argument),
        };
    }

    auto resolve_signature(
        Context&                       context,
        Scope&                         scope,
        hir::Environment_id const      environment,
        ast::Function_signature const& signature,
        kieli::Document_id const       document_id) -> hir::Function_signature
    {
        cpputil::always_assert(!signature.self_parameter.has_value()); // TODO

        Inference_state state { .document_id = document_id };

        auto template_parameters = resolve_template_parameters(
            context, state, scope, environment, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(context, state, scope, environment, parameter);
        };

        auto parameters = signature.function_parameters            //
                        | std::views::transform(resolve_parameter) //
                        | std::ranges::to<std::vector>();

        auto parameter_types = parameters //
                             | std::views::transform(&hir::Function_parameter::type)
                             | std::ranges::to<std::vector>();

        hir::Type const return_type
            = resolve_type(context, state, scope, environment, signature.return_type);

        ensure_no_unsolved_variables(context, state);

        return hir::Function_signature {
            .template_paramters = std::move(template_parameters),
            .parameters         = std::move(parameters),
            .return_type        = return_type,
            .function_type {
                context.hir.types.push(hir::type::Function {
                    .parameter_types = std::move(parameter_types),
                    .return_type     = return_type,
                }),
                signature.name.range,
            },
            .name = signature.name,
        };
    }
} // namespace

auto libresolve::resolve_function_body(Context& context, Function_info& info) -> hir::Function&
{
    resolve_function_signature(context, info);
    if (auto* const function = std::get_if<Function_with_resolved_signature>(&info.variant)) {
        Inference_state state { .document_id = info.document_id };
        hir::Expression body = resolve_expression(
            context, state, function->signature_scope, info.environment, function->unresolved_body);
        require_subtype_relationship(
            context,
            state,
            context.hir.types[body.type],
            context.hir.types[function->signature.return_type.id]);
        info.variant = hir::Function {
            .signature = std::move(function->signature),
            .body      = std::move(body),
        };
        ensure_no_unsolved_variables(context, state);
    }
    return std::get<hir::Function>(info.variant);
}

auto libresolve::resolve_function_signature(Context& context, Function_info& info)
    -> hir::Function_signature&
{
    if (auto* const function = std::get_if<ast::definition::Function>(&info.variant)) {
        Scope scope { info.document_id };
        info.variant = Function_with_resolved_signature {
            .unresolved_body = std::move(function->body),
            .signature       = resolve_signature(
                context, scope, info.environment, function->signature, info.document_id),
            .signature_scope = std::move(scope),
        };
    }
    if (auto* const function = std::get_if<Function_with_resolved_signature>(&info.variant)) {
        return function->signature;
    }
    return std::get<hir::Function>(info.variant).signature;
}

auto libresolve::resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&
{
    if (auto* const enumeration = std::get_if<ast::definition::Enumeration>(&info.variant)) {
        (void)context;
        (void)enumeration;
        cpputil::todo();
    }
    return std::get<hir::Enumeration>(info.variant);
}

auto libresolve::resolve_concept(Context& context, Concept_info& info) -> hir::Concept&
{
    if (auto* const concept_ = std::get_if<ast::definition::Concept>(&info.variant)) {
        (void)context;
        (void)concept_;
        cpputil::todo();
    }
    return std::get<hir::Concept>(info.variant);
}

auto libresolve::resolve_alias(Context& context, Alias_info& info) -> hir::Alias&
{
    if (auto* const alias = std::get_if<ast::definition::Alias>(&info.variant)) {
        (void)context;
        (void)alias;
        cpputil::todo();
    }
    return std::get<hir::Alias>(info.variant);
}

auto libresolve::resolve_function_bodies(Context& context, hir::Environment_id const environment)
    -> void
{
    for (Definition_variant const& variant : context.info.environments[environment].in_order) {
        if (auto* const id = std::get_if<hir::Function_id>(&variant)) {
            resolve_function_body(context, context.info.functions[*id]);
        }
        else if (auto* const id = std::get_if<hir::Module_id>(&variant)) {
            resolve_function_bodies(context, context.info.modules[*id].environment);
        }
    }
}

auto libresolve::resolve_definition(Context& context, Definition_variant const& definition) -> void
{
    std::visit(
        utl::Overload {
            [&](hir::Module_id const module) {
                resolve_definitions_in_order(
                    context, resolve_module(context, context.info.modules[module]));
            },
            [&](hir::Function_id const function) {
                (void)resolve_function_signature(context, context.info.functions[function]);
            },
            [&](hir::Enumeration_id const enumeration) {
                (void)resolve_enumeration(context, context.info.enumerations[enumeration]);
            },
            [&](hir::Concept_id const concept_) {
                (void)resolve_concept(context, context.info.concepts[concept_]);
            },
            [&](hir::Alias_id const alias) {
                (void)resolve_alias(context, context.info.aliases[alias]);
            },
        },
        definition);
}

auto libresolve::resolve_definitions_in_order(
    Context& context, hir::Environment_id const environment) -> void
{
    for (Definition_variant const& variant : context.info.environments[environment].in_order) {
        resolve_definition(context, variant);
    }
}
