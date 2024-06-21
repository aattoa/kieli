#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto resolve_function_parameter(
        Context&                       context,
        Inference_state&               state,
        Scope&                         scope,
        Environment_wrapper const      environment,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        cpputil::always_assert(parameter.type.has_value()); // TODO

        hir::Pattern pattern
            = resolve_pattern(context, state, scope, environment, *parameter.pattern);
        hir::Type const type
            = resolve_type(context, state, scope, environment, *parameter.type.value());

        require_subtype_relationship(
            context.compile_info.diagnostics, state, *pattern.type.variant, *type.variant);

        auto default_argument = parameter.default_argument.transform(
            [&](utl::Wrapper<ast::Expression> const argument) {
                hir::Expression expression
                    = resolve_expression(context, state, scope, environment, *argument);
                require_subtype_relationship(
                    context.compile_info.diagnostics,
                    state,
                    *expression.type.variant,
                    *type.variant);
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
        Environment_wrapper const      environment,
        ast::Function_signature const& signature,
        utl::Source_id const           source) -> hir::Function_signature
    {
        cpputil::always_assert(!signature.self_parameter.has_value()); // TODO

        Inference_state state { .source = source };

        auto template_parameters = resolve_template_parameters(
            context, state, scope, environment, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(context, state, scope, environment, parameter);
        };

        auto parameters = std::views::transform(signature.function_parameters, resolve_parameter)
                        | std::ranges::to<std::vector>();
        auto parameter_types = std::views::transform(parameters, &hir::Function_parameter::type)
                             | std::ranges::to<std::vector>();

        hir::Type const return_type
            = resolve_type(context, state, scope, environment, signature.return_type);

        ensure_no_unsolved_variables(context.compile_info, state);

        return hir::Function_signature {
            .template_paramters = std::move(template_parameters),
            .parameters         = std::move(parameters),
            .return_type        = return_type,
            .function_type {
                context.arenas.type(hir::type::Function {
                    .parameter_types = std::move(parameter_types),
                    .return_type     = return_type,
                }),
                signature.name.source_range,
            },
            .name = signature.name,
        };
    }
} // namespace

auto libresolve::resolve_function_body(Context& context, Function_info& info) -> hir::Function&
{
    resolve_function_signature(context, info);
    if (auto* const function = std::get_if<Function_with_resolved_signature>(&info.variant)) {
        Inference_state state { .source = info.environment->source };
        hir::Expression body = resolve_expression(
            context, state, function->signature_scope, info.environment, function->unresolved_body);
        require_subtype_relationship(
            context.compile_info.diagnostics,
            state,
            *body.type.variant,
            *function->signature.return_type.variant);
        info.variant = hir::Function {
            .signature = std::move(function->signature),
            .body      = std::move(body),
        };
        ensure_no_unsolved_variables(context.compile_info, state);
    }
    return std::get<hir::Function>(info.variant);
}

auto libresolve::resolve_function_signature(Context& context, Function_info& info)
    -> hir::Function_signature&
{
    if (auto* const function = std::get_if<ast::definition::Function>(&info.variant)) {
        Scope scope { info.environment->source };
        info.variant = Function_with_resolved_signature {
            .unresolved_body = std::move(function->body),
            .signature       = resolve_signature(
                context, scope, info.environment, function->signature, info.environment->source),
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

auto libresolve::resolve_typeclass(Context& context, Typeclass_info& info) -> hir::Typeclass&
{
    if (auto* const typeclass = std::get_if<ast::definition::Typeclass>(&info.variant)) {
        (void)context;
        (void)typeclass;
        cpputil::todo();
    }
    return std::get<hir::Typeclass>(info.variant);
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

auto libresolve::resolve_function_bodies(Context& context, Environment_wrapper const environment)
    -> void
{
    for (Definition_variant const& variant : environment->in_order) {
        if (auto* const info = std::get_if<utl::Mutable_wrapper<Function_info>>(&variant)) {
            resolve_function_body(context, info->as_mutable());
        }
        else if (auto* const info = std::get_if<utl::Mutable_wrapper<Module_info>>(&variant)) {
            resolve_function_bodies(context, (*info)->environment);
        }
    }
}

auto libresolve::resolve_definition(Context& context, Definition_variant const& definition) -> void
{
    std::visit(
        utl::Overload {
            [&](utl::Mutable_wrapper<Module_info> const module) {
                resolve_definitions_in_order(context, resolve_module(context, module.as_mutable()));
            },
            [&](utl::Mutable_wrapper<Function_info> const function) {
                (void)resolve_function_signature(context, function.as_mutable());
            },
            [&](utl::Mutable_wrapper<Enumeration_info> const enumeration) {
                (void)resolve_enumeration(context, enumeration.as_mutable());
            },
            [&](utl::Mutable_wrapper<Typeclass_info> const typeclass) {
                (void)resolve_typeclass(context, typeclass.as_mutable());
            },
            [&](utl::Mutable_wrapper<Alias_info> const alias) {
                (void)resolve_alias(context, alias.as_mutable());
            },
        },
        definition);
}

auto libresolve::resolve_definitions_in_order(
    Context& context, Environment_wrapper const environment) -> void
{
    for (Definition_variant const& variant : environment->in_order) {
        resolve_definition(context, variant);
    }
}
