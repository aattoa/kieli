#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace libresolve;

namespace {
    auto resolve_function_parameter(
        Context&                       context,
        Inference_state&               state,
        hir::Scope_id const            scope_id,
        hir::Environment_id const      environment_id,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        ast::Arena& ast = context.documents.at(state.document_id).ast;

        hir::Pattern pattern = resolve_pattern(
            context, state, scope_id, environment_id, ast.patterns[parameter.pattern]);
        hir::Type const type
            = resolve_type(context, state, scope_id, environment_id, ast.types[parameter.type]);

        require_subtype_relationship(
            context,
            state,
            pattern.range,
            context.hir.types[pattern.type],
            context.hir.types[type.id]);

        auto const resolve_default = [&](ast::Expression_id const argument) {
            hir::Expression expression = resolve_expression(
                context, state, scope_id, environment_id, ast.expressions[argument]);
            require_subtype_relationship(
                context,
                state,
                expression.range,
                context.hir.types[expression.type],
                context.hir.types[type.id]);
            return expression;
        };

        return hir::Function_parameter {
            .pattern          = std::move(pattern),
            .type             = type,
            .default_argument = parameter.default_argument.transform(resolve_default),
        };
    }

    auto resolve_signature(
        Context&                       context,
        hir::Scope_id const            scope_id,
        hir::Environment_id const      environment_id,
        kieli::Document_id const       document_id,
        ast::Function_signature const& signature) -> hir::Function_signature
    {
        Inference_state state { .document_id = document_id };

        auto template_parameters = resolve_template_parameters(
            context, state, scope_id, environment_id, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(context, state, scope_id, environment_id, parameter);
        };

        auto parameters = signature.function_parameters
                        | std::views::transform(resolve_parameter) //
                        | std::ranges::to<std::vector>();

        auto parameter_types = parameters //
                             | std::views::transform(&hir::Function_parameter::type)
                             | std::ranges::to<std::vector>();

        hir::Type const return_type
            = resolve_type(context, state, scope_id, environment_id, signature.return_type);

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
            .name     = signature.name,
            .scope_id = context.info.scopes.push(Scope { .document_id = document_id }),
        };
    }
} // namespace

auto libresolve::resolve_function_body(Context& context, Function_info& info) -> hir::Expression&
{
    if (not info.body.has_value()) {
        hir::Function_signature& signature = resolve_function_signature(context, info);
        Inference_state          state { .document_id = info.document_id };
        info.body = resolve_expression(
            context, state, signature.scope_id, info.environment_id, info.ast.body);
        require_subtype_relationship(
            context,
            state,
            info.body.value().range,
            context.hir.types[info.body.value().type],
            context.hir.types[signature.return_type.id]);
        ensure_no_unsolved_variables(context, state);
    }
    return info.body.value();
}

auto libresolve::resolve_function_signature(Context& context, Function_info& info)
    -> hir::Function_signature&
{
    if (not info.signature.has_value()) {
        scope(context, Scope { .document_id = info.document_id }, [&](hir::Scope_id scope_id) {
            info.signature = resolve_signature(
                context, scope_id, info.environment_id, info.document_id, info.ast.signature);
            return 0; // dummy return
        });
    }
    return info.signature.value();
}

auto libresolve::resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&
{
    if (not info.hir.has_value()) {
        (void)context;
        cpputil::todo();
    }
    return info.hir.value();
}

auto libresolve::resolve_concept(Context& context, Concept_info& info) -> hir::Concept&
{
    if (not info.hir.has_value()) {
        (void)context;
        cpputil::todo();
    }
    return info.hir.value();
}

auto libresolve::resolve_alias(Context& context, Alias_info& info) -> hir::Alias&
{
    if (not info.hir.has_value()) {
        (void)context;
        cpputil::todo();
    }
    return info.hir.value();
}
