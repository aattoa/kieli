#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    struct Template_parameter_resolution_visitor {
        Context&                    ctx;
        Inference_state&            state;
        hir::Scope_id               scope_id;
        hir::Environment_id         env_id;
        hir::Template_parameter_tag tag;

        auto ast() -> ast::Arena&
        {
            return ctx.documents.at(state.doc_id).ast;
        }

        template <typename Default>
        auto resolve_default_argument()
        {
            return [&](auto const& argument) {
                auto const visitor = utl::Overload {
                    [&](ast::Wildcard const&) {
                        return hir::Wildcard {}; //
                    },
                    [&](ast::Type_id const type) -> hir::Type {
                        return resolve_type(ctx, state, scope_id, env_id, ast().type[type]);
                    },
                    [&](ast::Mutability const& mutability) {
                        return resolve_mutability(ctx, scope_id, mutability);
                    },
                };
                return std::visit<Default>(visitor, argument);
            };
        }

        auto operator()(ast::Template_type_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            bind_type(
                ctx.info.scopes.index_vector[scope_id],
                Type_bind {
                    .name = parameter.name,
                    .type = ctx.hir.type.push(hir::type::Parameterized {
                        .tag = tag,
                        .id  = parameter.name.id,
                    }),
                });
            auto const resolve_concept = [&](ast::Path const& concept_path) {
                return resolve_concept_reference(ctx, state, scope_id, env_id, concept_path);
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
                .id    = ctx.hir.mut.push(hir::mutability::Parameterized { tag }),
                .range = parameter.name.range,
            };
            bind_mutability(
                ctx.info.scopes.index_vector[scope_id],
                Mutability_bind { .name = parameter.name, .mutability = mutability });
            return hir::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_mutability_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_value_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            ki::add_error(
                ctx.db,
                state.doc_id,
                parameter.name.range,
                "Template value parameters are not supported yet");
            return hir::Template_value_parameter {
                .type = error_type(ctx.constants, parameter.name.range),
                .name = parameter.name,
            };
        }
    };
} // namespace

auto ki::resolve::resolve_template_parameters(
    Context&                        ctx,
    Inference_state&                state,
    hir::Scope_id const             scope_id,
    hir::Environment_id const       env_id,
    ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>
{
    auto const resolve_parameter = [&](ast::Template_parameter const& parameter) {
        auto const tag = fresh_template_parameter_tag(ctx.tags);
        return hir::Template_parameter {
            .variant = std::visit(
                Template_parameter_resolution_visitor {
                    .ctx      = ctx,
                    .state    = state,
                    .scope_id = scope_id,
                    .env_id   = env_id,
                    .tag      = tag,
                },
                parameter.variant),
            .tag   = tag,
            .range = parameter.range,
        };
    };
    auto const resolve = std::views::transform(resolve_parameter) | std::ranges::to<std::vector>();
    return parameters.transform(resolve).value_or(std::vector<hir::Template_parameter> {});
}

auto ki::resolve::resolve_template_arguments(
    Context&                                    ctx,
    Inference_state&                            state,
    hir::Scope_id const                         scope_id,
    hir::Environment_id const                   env_id,
    std::vector<hir::Template_parameter> const& parameters,
    std::vector<ast::Template_argument> const&  arguments) -> std::vector<hir::Template_argument>
{
    (void)ctx;
    (void)state;
    (void)scope_id;
    (void)env_id;
    (void)parameters;
    (void)arguments;
    cpputil::todo();
}
