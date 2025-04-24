#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Collector {
        db::Database&       db;
        Context&            ctx;
        des::Context&       des_ctx;
        hir::Environment_id env_id;

        void bind_symbol(db::Name name, hir::Symbol symbol)
        {
            auto& env = ctx.hir.environments[env_id];
            if (env.map.contains(name.id)) {
                auto message = std::format("Redefinition of '{}'", db.string_pool.get(name.id));
                db::add_error(db, ctx.doc_id, name.range, std::move(message));
            }
            else {
                env.map.insert({ name.id, symbol });
            }
            env.in_order.push_back(symbol);
        }

        void operator()(cst::Function& cst)
        {
            auto name = cst.signature.name;
            auto ast  = des::desugar_function(des_ctx, cst);

            auto fun_id = ctx.hir.functions.push(hir::Function_info {
                .cst       = std::move(cst),
                .ast       = std::move(ast),
                .signature = std::nullopt,
                .body      = std::nullopt,
                .env_id    = env_id,
                .doc_id    = ctx.doc_id,
                .name      = name,
            });

            bind_symbol(name, fun_id);
        }

        void operator()(cst::Struct& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_struct(des_ctx, cst);

            auto type_id = ctx.hir.types.push(db::Error {}); // Overridden below
            auto enum_id = ctx.hir.enumerations.push(hir::Enumeration_info {
                .cst     = std::move(cst),
                .ast     = std::move(ast),
                .hir     = std::nullopt,
                .type_id = type_id,
                .env_id  = env_id,
                .doc_id  = ctx.doc_id,
                .name    = name,
            });

            ctx.hir.types[type_id] = hir::type::Enumeration { .name = name, .id = enum_id };

            bind_symbol(name, enum_id);
        }

        void operator()(cst::Enum& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_enum(des_ctx, cst);

            auto type_id = ctx.hir.types.push(db::Error {}); // Overridden below
            auto enum_id = ctx.hir.enumerations.push(hir::Enumeration_info {
                .cst     = std::move(cst),
                .ast     = std::move(ast),
                .hir     = std::nullopt,
                .type_id = type_id,
                .env_id  = env_id,
                .doc_id  = ctx.doc_id,
                .name    = name,
            });

            ctx.hir.types[type_id] = hir::type::Enumeration { .name = name, .id = enum_id };

            bind_symbol(name, enum_id);
        }

        void operator()(cst::Alias& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_alias(des_ctx, cst);

            auto alias_id = ctx.hir.aliases.push(hir::Alias_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = ctx.doc_id,
                .name   = name,
            });

            bind_symbol(name, alias_id);
        }

        void operator()(cst::Concept& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_concept(des_ctx, cst);

            auto concept_id = ctx.hir.concepts.push(hir::Concept_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = ctx.doc_id,
                .name   = name,
            });

            bind_symbol(name, concept_id);
        }

        void operator()(cst::Impl& impl)
        {
            // TODO
            db::add_error(
                db,
                ctx.doc_id,
                des_ctx.cst.ranges[impl.impl_token],
                "impl blocks are not supported yet");
        }

        void operator()(cst::Submodule& module)
        {
            if (module.template_parameters.has_value()) {
                cpputil::todo();
            }

            auto child_env_id = ctx.hir.environments.push(hir::Environment {
                .map       = {},
                .in_order  = {},
                .parent_id = env_id,
                .name_id   = module.name.id,
                .doc_id    = ctx.doc_id,
            });

            auto module_id = ctx.hir.modules.push(hir::Module_info {
                .mod_env_id = child_env_id,
                .env_id     = env_id,
                .doc_id     = ctx.doc_id,
                .name       = module.name,
            });

            bind_symbol(module.name, module_id);

            auto collector = Collector {
                .db      = db,
                .ctx     = ctx,
                .des_ctx = des_ctx,
                .env_id  = child_env_id,
            };

            for (cst::Definition& definition : module.definitions.value) {
                std::visit(collector, definition.variant);
            }
        }
    };
} // namespace

auto ki::res::collect_document(db::Database& db, Context& ctx, db::Document_id doc_id)
    -> hir::Environment_id
{
    auto module = par::parse(db, doc_id);

    auto env_id = ctx.hir.environments.push(hir::Environment {
        .map       = {},
        .in_order  = {},
        .parent_id = std::nullopt,
        .name_id   = std::nullopt,
        .doc_id    = doc_id,
    });

    auto des_ctx   = des::context(db, doc_id);
    auto collector = Collector { .db = db, .ctx = ctx, .des_ctx = des_ctx, .env_id = env_id };

    for (cst::Definition& definition : module.definitions) {
        std::visit(collector, definition.variant);
    }

    db.documents[doc_id].arena.ast = std::move(des_ctx.ast);

    return env_id;
}
