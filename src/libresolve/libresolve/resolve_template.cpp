#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&               db;
        Context&                    ctx;
        Block_state&                state;
        db::Environment_id          env_id;
        hir::Template_parameter_tag tag;

        template <typename Default>
        auto resolve_default_argument()
        {
            return [&](auto const& argument) {
                auto const visitor = utl::Overload {
                    [&](ast::Wildcard const&) {
                        return hir::Wildcard {}; //
                    },
                    [&](ast::Type_id const type) {
                        return resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[type]);
                    },
                    [&](ast::Mutability const& mutability) {
                        return resolve_mutability(db, ctx, env_id, mutability);
                    },
                };
                return std::visit<Default>(visitor, argument);
            };
        }

        auto operator()(ast::Template_type_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            auto const local_id = ctx.arena.hir.local_types.push(
                hir::Local_type {
                    .name    = parameter.name,
                    .type_id = ctx.arena.hir.types.push(
                        hir::type::Parameterized {
                            .tag = tag,
                            .id  = parameter.name.id,
                        }),
                });
            bind_symbol(db, ctx, env_id, parameter.name, local_id);

            if (not parameter.concepts.empty()) {
                lsp::Range  range   = parameter.concepts.front().head().name.range;
                std::string message = "Concept requirements are not supported yet";
                ctx.add_diagnostic(lsp::error(range, std::move(message)));
            }

            return hir::Template_type_parameter {
                .concept_ids      = {},
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_type_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_mutability_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            auto const local_id = ctx.arena.hir.local_mutabilities.push(
                hir::Local_mutability {
                    .name   = parameter.name,
                    .mut_id = ctx.arena.hir.mutabilities.push(hir::mut::Parameterized { tag }),
                });
            bind_symbol(db, ctx, env_id, parameter.name, local_id);
            return hir::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_mutability_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_value_parameter const& parameter)
            -> hir::Template_parameter_variant
        {
            std::string message = "Template value parameters are not supported yet";
            ctx.add_diagnostic(lsp::error(parameter.name.range, std::move(message)));
            return hir::Template_value_parameter {
                .type = ctx.builtins.type_error,
                .name = parameter.name,
            };
        }
    };
} // namespace

auto ki::res::resolve_template_parameters(
    db::Database&                   db,
    Context&                        ctx,
    Block_state&                    state,
    db::Environment_id              env_id,
    ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>
{
    auto const resolve_parameter = [&](ast::Template_parameter const& parameter) {
        auto const tag = fresh_template_parameter_tag(ctx.tags);
        return hir::Template_parameter {
            .variant = std::visit(
                Visitor {
                    .db     = db,
                    .ctx    = ctx,
                    .state  = state,
                    .env_id = env_id,
                    .tag    = tag,
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
    Block_state&                                state,
    db::Environment_id                          env_id,
    std::vector<hir::Template_parameter> const& parameters,
    std::vector<ast::Template_argument> const&  arguments) -> std::vector<hir::Template_argument>
{
    (void)db;
    (void)ctx;
    (void)state;
    (void)env_id;
    (void)parameters;
    (void)arguments;
    cpputil::todo();
}
