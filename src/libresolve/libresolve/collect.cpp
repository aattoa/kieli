#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    struct Collector {
        Context&             ctx;
        ki::desugar::Context des_ctx;
        ki::Document_id      doc_id;
        hir::Environment_id  env_id;

        void operator()(cst::Function& cst) const
        {
            auto name = cst.signature.name;
            auto ast  = ki::desugar::desugar_function(des_ctx, cst);

            auto const id = ctx.info.functions.push(Function_info {
                .cst       = std::move(cst),
                .ast       = std::move(ast),
                .signature = std::nullopt,
                .body      = std::nullopt,
                .env_id    = env_id,
                .doc_id    = doc_id,
                .name      = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.lower_map.emplace_back(
                name.id, Lower_info { .name = name, .doc_id = doc_id, .variant = id });
        }

        void operator()(cst::Struct& cst) const
        {
            auto name = cst.name;
            auto ast  = ki::desugar::desugar_struct(des_ctx, cst);

            auto const id = ctx.info.enumerations.push(Enumeration_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Enum& cst) const
        {
            auto name = cst.name;
            auto ast  = ki::desugar::desugar_enum(des_ctx, cst);

            auto const id = ctx.info.enumerations.push(Enumeration_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Alias& cst) const
        {
            auto name = cst.name;
            auto ast  = ki::desugar::desugar_alias(des_ctx, cst);

            auto const id = ctx.info.aliases.push(Alias_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Concept& cst) const
        {
            auto name = cst.name;
            auto ast  = ki::desugar::desugar_concept(des_ctx, cst);

            auto const id = ctx.info.concepts.push(Concept_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = doc_id,
                .name   = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(
                name.id, Upper_info { .name = name, .doc_id = env.doc_id, .variant = id });
        }

        void operator()(cst::Impl&) const
        {
            cpputil::todo();
        }

        void operator()(cst::Submodule&) const
        {
            cpputil::todo();
        }
    };
} // namespace

auto ki::resolve::collect_document(Context& ctx, ki::Document_id doc_id) -> hir::Environment_id
{
    auto cst  = ki::parse::parse(ctx.db, doc_id);
    auto info = Document_info { .cst = std::move(cst.arena), .ast = {} };

    auto env_id = ctx.info.environments.push(Environment {
        .upper_map = {},
        .lower_map = {},
        .in_order  = {},
        .parent_id = std::nullopt,
        .doc_id    = doc_id,
    });

    ki::desugar::Context des_ctx {
        .db     = ctx.db,
        .cst    = info.cst,
        .ast    = info.ast,
        .doc_id = doc_id,
    };
    Collector collector {
        .ctx     = ctx,
        .des_ctx = des_ctx,
        .doc_id  = doc_id,
        .env_id  = env_id,
    };

    for (cst::Definition& definition : cst.definitions) {
        std::visit(collector, definition.variant);
    }

    ctx.documents.insert_or_assign(doc_id, std::move(info));
    return env_id;
}
