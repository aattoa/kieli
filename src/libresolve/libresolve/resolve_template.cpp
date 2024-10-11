#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    struct Template_parameter_resolution_visitor {
        Context&                    context;
        Inference_state&            state;
        hir::Scope_id               scope_id;
        hir::Environment_id         environment_id;
        hir::Template_parameter_tag parameter_id;

        auto ast() -> ast::Arena&
        {
            return context.documents.at(state.document_id).ast;
        }

        template <class Default>
        auto resolve_default_argument()
        {
            return [&](auto const& argument) {
                auto const visitor = utl::Overload {
                    [&](ast::Wildcard const&) {
                        return hir::Wildcard {}; //
                    },
                    [&](ast::Type_id const type) -> hir::Type {
                        return resolve_type(
                            context, state, scope_id, environment_id, ast().types[type]);
                    },
                    [&](ast::Mutability const& mutability) {
                        return resolve_mutability(context, scope_id, mutability);
                    },
                };
                return std::visit<Default>(visitor, argument);
            };
        }

        auto operator()(ast::Template_type_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            Type_bind bind {
                parameter.name,
                context.hir.types.push(hir::type::Parameterized { parameter_id }),
            };
            bind_type(
                context.info.scopes.index_vector[scope_id],
                parameter.name.identifier,
                std::move(bind));
            auto const resolve_concept = [&](ast::Path const& concept_path) {
                return resolve_concept_reference(
                    context, state, scope_id, environment_id, concept_path);
            };
            return hir::Template_type_parameter {
                .concept_ids = std::ranges::to<std::vector>(
                    std::views::transform(parameter.concepts, resolve_concept)),
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_type_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_mutability_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            hir::Mutability mutability {
                context.hir.mutabilities.push(hir::mutability::Parameterized { parameter_id }),
                parameter.name.range,
            };
            Mutability_bind bind { .name = parameter.name, .mutability = std::move(mutability) };
            bind_mutability(
                context.info.scopes.index_vector[scope_id],
                parameter.name.identifier,
                std::move(bind));
            return hir::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_mutability_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_value_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            kieli::add_error(
                context.db,
                state.document_id,
                parameter.name.range,
                "Template value parameters are not supported yet");
            return hir::Template_value_parameter {
                .type = error_type(context.constants, parameter.name.range),
                .name = parameter.name,
            };
        }
    };
} // namespace

auto libresolve::resolve_template_parameters(
    Context&                        context,
    Inference_state&                state,
    hir::Scope_id const             scope_id,
    hir::Environment_id const       environment_id,
    ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>
{
    auto const resolve_parameter = [&](ast::Template_parameter const& parameter) {
        auto const tag = context.tags.fresh_template_parameter_tag();
        return hir::Template_parameter {
            .variant = std::visit(
                Template_parameter_resolution_visitor {
                    context, state, scope_id, environment_id, tag },
                parameter.variant),
            .tag   = tag,
            .range = parameter.range,
        };
    };
    auto const resolve = std::views::transform(resolve_parameter) | std::ranges::to<std::vector>();
    return parameters.transform(resolve).value_or(std::vector<hir::Template_parameter> {});
}

auto libresolve::resolve_template_arguments(
    Context&                                    context,
    Inference_state&                            state,
    hir::Scope_id const                         scope_id,
    hir::Environment_id const                   environment_id,
    std::vector<hir::Template_parameter> const& parameters,
    std::vector<ast::Template_argument> const&  arguments) -> std::vector<hir::Template_argument>
{
    (void)context;
    (void)state;
    (void)scope_id;
    (void)environment_id;
    (void)parameters;
    (void)arguments;
    cpputil::todo();
}
