#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&               db;
        Context&                    ctx;
        Inference_state&            state;
        Scope_id                    scope_id;
        hir::Environment_id         env_id;
        hir::Template_parameter_tag tag;

        auto ast() -> ast::Arena&
        {
            return db.documents[state.doc_id].ast;
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
                        return resolve_type(db, ctx, state, scope_id, env_id, ast().types[type]);
                    },
                    [&](ast::Mutability const& mutability) {
                        return resolve_mutability(db, ctx, scope_id, mutability);
                    },
                };
                return std::visit<Default>(visitor, argument);
            };
        }

        auto operator()(ast::Template_type_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            hir::Local_type local {
                .name    = parameter.name,
                .type_id = ctx.hir.types.push(hir::type::Parameterized {
                    .tag = tag,
                    .id  = parameter.name.id,
                }),
                .unused  = true,
            };

            bind_local_type(db, ctx, scope_id, parameter.name, std::move(local));

            auto const resolve_concept = [&](ast::Path const& concept_path) {
                return resolve_concept_reference(db, ctx, state, scope_id, env_id, concept_path);
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
            hir::Local_mutability local {
                .name   = parameter.name,
                .mut_id = ctx.hir.mutabilities.push(hir::mut::Parameterized { tag }),
                .unused = true,
            };
            bind_local_mutability(db, ctx, scope_id, parameter.name, std::move(local));

            return hir::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_mutability_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_value_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            db::add_error(
                db,
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

auto ki::res::resolve_template_parameters(
    db::Database&                   db,
    Context&                        ctx,
    Inference_state&                state,
    Scope_id const                  scope_id,
    hir::Environment_id const       env_id,
    ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>
{
    auto const resolve_parameter = [&](ast::Template_parameter const& parameter) {
        auto const tag = fresh_template_parameter_tag(ctx.tags);
        return hir::Template_parameter {
            .variant = std::visit(
                Visitor {
                    .db       = db,
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

auto ki::res::resolve_template_arguments(
    db::Database&                               db,
    Context&                                    ctx,
    Inference_state&                            state,
    Scope_id const                              scope_id,
    hir::Environment_id const                   env_id,
    std::vector<hir::Template_parameter> const& parameters,
    std::vector<ast::Template_argument> const&  arguments) -> std::vector<hir::Template_argument>
{
    (void)db;
    (void)ctx;
    (void)state;
    (void)scope_id;
    (void)env_id;
    (void)parameters;
    (void)arguments;
    cpputil::todo();
}
