#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Collector {
        std::vector<db::Symbol_id>& symbol_order;
        db::Database&               db;
        Context&                    ctx;
        des::Context&               des_ctx;
        db::Environment_id          env_id;

        void bind(db::Name name, db::Symbol_variant variant)
        {
            symbol_order.push_back(bind_symbol(db, ctx, env_id, name, variant));
        }

        void operator()(cst::Function& cst)
        {
            auto name = cst.signature.name;
            auto ast  = des::desugar_function(des_ctx, cst);

            auto fun_id = ctx.arena.hir.functions.push(hir::Function_info {
                .cst       = std::move(cst),
                .ast       = std::move(ast),
                .signature = std::nullopt,
                .body      = std::nullopt,
                .env_id    = env_id,
                .doc_id    = ctx.doc_id,
                .name      = name,
            });

            bind(name, fun_id);
        }

        void operator()(cst::Struct& cst)
        {
            auto name = cst.constructor.name;
            auto ast  = des::desugar_struct(des_ctx, cst);

            auto type_id   = ctx.arena.hir.types.push(db::Error {}); // Overwritten below
            auto struct_id = ctx.arena.hir.structures.push(hir::Structure_info {
                .cst     = std::move(cst),
                .ast     = std::move(ast),
                .hir     = std::nullopt,
                .type_id = type_id,
                .env_id  = env_id,
                .doc_id  = ctx.doc_id,
                .name    = name,
            });

            ctx.arena.hir.types[type_id] = hir::type::Structure { .name = name, .id = struct_id };

            bind(name, struct_id);
        }

        void operator()(cst::Enum& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_enum(des_ctx, cst);

            auto type_id = ctx.arena.hir.types.push(db::Error {}); // Overwritten below
            auto enum_id = ctx.arena.hir.enumerations.push(hir::Enumeration_info {
                .cst     = std::move(cst),
                .ast     = std::move(ast),
                .hir     = std::nullopt,
                .type_id = type_id,
                .env_id  = env_id,
                .doc_id  = ctx.doc_id,
                .name    = name,
            });

            ctx.arena.hir.types[type_id] = hir::type::Enumeration { .name = name, .id = enum_id };

            bind(name, enum_id);
        }

        void operator()(cst::Alias& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_alias(des_ctx, cst);

            auto alias_id = ctx.arena.hir.aliases.push(hir::Alias_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = ctx.doc_id,
                .name   = name,
            });

            bind(name, alias_id);
        }

        void operator()(cst::Concept& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_concept(des_ctx, cst);

            auto concept_id = ctx.arena.hir.concepts.push(hir::Concept_info {
                .cst    = std::move(cst),
                .ast    = std::move(ast),
                .hir    = std::nullopt,
                .env_id = env_id,
                .doc_id = ctx.doc_id,
                .name   = name,
            });

            bind(name, concept_id);
        }

        void operator()(cst::Impl& impl)
        {
            db::add_error(
                db,
                ctx.doc_id,
                des_ctx.cst.ranges[impl.impl_token],
                "impl blocks are not supported yet");
        }

        void operator()(cst::Submodule& module)
        {
            if (module.template_parameters.has_value()) {
                db::add_error(
                    db,
                    ctx.doc_id,
                    module.name.range,
                    "Module template parameters are not supported yet");
            }

            auto child_env_id = ctx.arena.environments.push(db::Environment {
                .map       = {},
                .parent_id = env_id,
                .name_id   = module.name.id,
                .doc_id    = ctx.doc_id,
                .kind      = db::Environment_kind::Module,
            });

            auto module_id = ctx.arena.hir.modules.push(hir::Module_info {
                .mod_env_id = child_env_id,
                .env_id     = env_id,
                .doc_id     = ctx.doc_id,
                .name       = module.name,
            });

            bind(module.name, module_id);

            auto collector = Collector {
                .symbol_order = symbol_order,
                .db           = db,
                .ctx          = ctx,
                .des_ctx      = des_ctx,
                .env_id       = child_env_id,
            };

            for (cst::Definition& definition : module.definitions.value) {
                std::visit(collector, definition.variant);
            }
        }
    };
} // namespace

auto ki::res::collect_document(db::Database& db, Context& ctx) -> std::vector<db::Symbol_id>
{
    std::vector<db::Symbol_id> symbol_order;

    auto module  = par::parse(db, ctx.doc_id);
    auto des_ctx = des::context(db, ctx.doc_id);

    auto collector = Collector {
        .symbol_order = symbol_order,
        .db           = db,
        .ctx          = ctx,
        .des_ctx      = des_ctx,
        .env_id       = ctx.root_env_id,
    };

    for (cst::Definition& definition : module.definitions) {
        std::visit(collector, definition.variant);
    }

    ctx.arena.cst = std::move(db.documents[ctx.doc_id].arena.cst);
    ctx.arena.ast = std::move(des_ctx.ast);

    return symbol_order;
}
