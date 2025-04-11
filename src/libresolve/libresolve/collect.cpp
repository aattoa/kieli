#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Collector {
        Context&            ctx;
        des::Context        des_ctx;
        db::Document_id     doc_id;
        hir::Environment_id env_id;

        void operator()(cst::Function& cst)
        {
            auto name = cst.signature.name;
            auto ast  = des::desugar_function(des_ctx, cst);

            auto const id = ctx.hir.functions.push(hir::Function_info {
                .cst       = std::move(cst),
                .ast       = std::move(ast),
                .signature = std::nullopt,
                .body      = std::nullopt,
                .env_id    = env_id,
                .doc_id    = doc_id,
                .name      = name,
            });

            auto& env = ctx.hir.environments[env_id];
            env.in_order.emplace_back(id);
            env.lower_map.emplace_back(
                name.id, hir::Lower_info { .name = name, .doc_id = doc_id, .variant = id });
        }

        void operator()(cst::Struct& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_struct(des_ctx, cst);

            auto const id = ctx.hir.enumerations.push(hir::Enumeration_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.hir.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, hir::Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Enum& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_enum(des_ctx, cst);

            auto const id = ctx.hir.enumerations.push(hir::Enumeration_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.hir.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, hir::Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Alias& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_alias(des_ctx, cst);

            auto const id = ctx.hir.aliases.push(hir::Alias_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.hir.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, hir::Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Concept& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_concept(des_ctx, cst);

            auto const id = ctx.hir.concepts.push(hir::Concept_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.hir.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, hir::Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Impl&)
        {
            cpputil::todo();
        }

        void operator()(cst::Submodule&)
        {
            cpputil::todo();
        }
    };
} // namespace

auto ki::res::collect_document(Context& ctx, db::Document_id doc_id) -> hir::Environment_id
{
    auto module = par::parse(ctx.db, doc_id);

    auto env_id = ctx.hir.environments.push(hir::Environment {
        .upper_map = {},
        .lower_map = {},
        .in_order  = {},
        .parent_id = std::nullopt,
        .doc_id    = doc_id,
    });

    Collector collector {
        .ctx     = ctx,
        .des_ctx = des::context(ctx.db, doc_id),
        .doc_id  = doc_id,
        .env_id  = env_id,
    };

    for (cst::Definition& definition : module.definitions) {
        std::visit(collector, definition.variant);
    }

    ctx.db.documents[doc_id].ast = std::move(collector.des_ctx.ast);

    return env_id;
}
