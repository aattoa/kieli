#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>

using namespace libresolve;

namespace {
    struct Collector {
        Context&               ctx;
        kieli::Desugar_context des_ctx;
        kieli::Document_id     doc_id;
        hir::Environment_id    env_id;

        void operator()(cst::definition::Function& cst) const
        {
            auto name = cst.signature.name;
            auto ast  = kieli::desugar_function(des_ctx, cst);

            auto const id = ctx.info.functions.push(Function_info {
                .cst            = std::move(cst),
                .ast            = std::move(ast),
                .environment_id = env_id,
                .document_id    = doc_id,
                .name           = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.lower_map.emplace_back(name.identifier, Lower_info { name, doc_id, id });
        }

        void operator()(cst::definition::Struct& cst) const
        {
            auto name = cst.name;
            auto ast  = kieli::desugar_struct(des_ctx, cst);

            auto const id = ctx.info.enumerations.push(Enumeration_info {
                .cst            = std::move(cst),
                .ast            = std::move(ast),
                .environment_id = env_id,
                .document_id    = doc_id,
                .name           = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(name.identifier, Upper_info { name, env.document_id, id });
        }

        void operator()(cst::definition::Enum& cst) const
        {
            auto name = cst.name;
            auto ast  = kieli::desugar_enum(des_ctx, cst);

            auto const id = ctx.info.enumerations.push(Enumeration_info {
                .cst            = std::move(cst),
                .ast            = std::move(ast),
                .environment_id = env_id,
                .document_id    = doc_id,
                .name           = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(name.identifier, Upper_info { name, env.document_id, id });
        }

        void operator()(cst::definition::Alias& cst) const
        {
            auto name = cst.name;
            auto ast  = kieli::desugar_alias(des_ctx, cst);

            auto const id = ctx.info.aliases.push(Alias_info {
                .cst            = std::move(cst),
                .ast            = std::move(ast),
                .environment_id = env_id,
                .document_id    = doc_id,
                .name           = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(name.identifier, Upper_info { name, env.document_id, id });
        }

        void operator()(cst::definition::Concept& cst) const
        {
            auto name = cst.name;
            auto ast  = kieli::desugar_concept(des_ctx, cst);

            auto const id = ctx.info.concepts.push(Concept_info {
                .cst            = std::move(cst),
                .ast            = std::move(ast),
                .environment_id = env_id,
                .document_id    = doc_id,
                .name           = name,
            });

            auto& env = ctx.info.environments[env_id];
            env.in_order.emplace_back(id);
            env.upper_map.emplace_back(name.identifier, Upper_info { name, env.document_id, id });
        }

        void operator()(cst::definition::Impl&) const
        {
            cpputil::todo();
        }

        void operator()(cst::definition::Submodule&) const
        {
            cpputil::todo();
        }
    };
} // namespace

auto libresolve::collect_document(Context& context, kieli::Document_id id) -> hir::Environment_id
{
    auto cst  = kieli::parse(context.db, id);
    auto env  = context.info.environments.push(Environment { .document_id = id });
    auto info = Document_info { .cst = std::move(cst.get().arena) };

    Collector collector {
        .ctx     = context,
        .des_ctx = kieli::Desugar_context { context.db, info.cst, info.ast, id },
        .doc_id  = id,
        .env_id  = env,
    };
    for (cst::Definition& definition : cst.get().definitions) {
        std::visit(collector, definition.variant);
    }

    context.documents.insert_or_assign(id, std::move(info));
    return env;
}
