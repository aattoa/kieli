#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Collector {
        db::Database&                   db;
        Context&                        ctx;
        des::Context&                   des_ctx;
        std::vector<db::Environment_id> env_id_stack;
        std::vector<db::Symbol_id>      symbol_order;

        auto env_id() const -> db::Environment_id
        {
            cpputil::always_assert(not env_id_stack.empty());
            return env_id_stack.back();
        }

        void bind(db::Name name, db::Symbol_variant variant)
        {
            symbol_order.push_back(bind_symbol(db, ctx, env_id(), name, variant));
        }

        void operator()(cst::Function const& cst)
        {
            auto name = cst.signature.name;
            auto ast  = des::desugar_function(des_ctx, cst);

            auto fun_id = ctx.arena.hir.functions.push(
                hir::Function_info {
                    .ast       = std::move(ast),
                    .signature = std::nullopt,
                    .body_id   = std::nullopt,
                    .env_id    = env_id(),
                    .doc_id    = ctx.doc_id,
                    .name      = name,
                });

            bind(name, fun_id);
        }

        void operator()(cst::Struct const& cst)
        {
            auto name = cst.constructor.name;
            auto ast  = des::desugar_struct(des_ctx, cst);

            auto type_id   = ctx.arena.hir.types.push(db::Error {}); // Overwritten below
            auto struct_id = ctx.arena.hir.structures.push(
                hir::Structure_info {
                    .ast     = std::move(ast),
                    .hir     = std::nullopt,
                    .type_id = type_id,
                    .env_id  = env_id(),
                    .doc_id  = ctx.doc_id,
                    .name    = name,
                });

            ctx.arena.hir.types[type_id] = hir::type::Structure { .name = name, .id = struct_id };

            bind(name, struct_id);
        }

        void operator()(cst::Enum const& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_enum(des_ctx, cst);

            auto type_id = ctx.arena.hir.types.push(db::Error {}); // Overwritten below
            auto enum_id = ctx.arena.hir.enumerations.push(
                hir::Enumeration_info {
                    .ast     = std::move(ast),
                    .hir     = std::nullopt,
                    .type_id = type_id,
                    .env_id  = env_id(),
                    .doc_id  = ctx.doc_id,
                    .name    = name,
                });

            ctx.arena.hir.types[type_id] = hir::type::Enumeration { .name = name, .id = enum_id };

            bind(name, enum_id);
        }

        void operator()(cst::Alias const& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_alias(des_ctx, cst);

            auto alias_id = ctx.arena.hir.aliases.push(
                hir::Alias_info {
                    .ast    = std::move(ast),
                    .hir    = std::nullopt,
                    .env_id = env_id(),
                    .doc_id = ctx.doc_id,
                    .name   = name,
                });

            bind(name, alias_id);
        }

        void operator()(cst::Concept const& cst)
        {
            auto name = cst.name;
            auto ast  = des::desugar_concept(des_ctx, cst);

            auto concept_id = ctx.arena.hir.concepts.push(
                hir::Concept_info {
                    .ast    = std::move(ast),
                    .hir    = std::nullopt,
                    .env_id = env_id(),
                    .doc_id = ctx.doc_id,
                    .name   = name,
                });

            bind(name, concept_id);
        }

        void operator()(cst::Impl_begin const& impl)
        {
            db::add_error(db, ctx.doc_id, impl.impl_token, "impl blocks are not supported yet");
        }

        void operator()(cst::Submodule_begin const& module)
        {
            auto child_env_id = ctx.arena.environments.push(
                db::Environment {
                    .map       = {},
                    .parent_id = env_id(),
                    .name_id   = module.name.id,
                    .doc_id    = ctx.doc_id,
                    .kind      = db::Environment_kind::Module,
                });

            auto module_id = ctx.arena.hir.modules.push(
                hir::Module_info {
                    .mod_env_id = child_env_id,
                    .env_id     = env_id(),
                    .doc_id     = ctx.doc_id,
                    .name       = module.name,
                });

            bind(module.name, module_id);

            env_id_stack.push_back(child_env_id);
        }

        void operator()(cst::Block_end const&)
        {
            cpputil::always_assert(not env_id_stack.empty());
            env_id_stack.pop_back();
        }
    };
} // namespace

auto ki::res::collect_document(db::Database& db, Context& ctx) -> std::vector<db::Symbol_id>
{
    auto par_ctx = par::context(db, ctx.doc_id);

    auto des_ctx = des::Context {
        .db     = db,
        .doc_id = ctx.doc_id,
        .cst    = par_ctx.arena,
        .ast    = ast::Arena {},
    };

    auto collector = Collector {
        .db           = db,
        .ctx          = ctx,
        .des_ctx      = des_ctx,
        .env_id_stack = { ctx.root_env_id },
        .symbol_order = {},
    };

    par::parse(par_ctx, collector);

    ctx.arena.ast = std::move(des_ctx.ast);

    if (db.config.semantic_tokens == db::Semantic_token_mode::Full) {
        db.documents[ctx.doc_id].info.semantic_tokens = std::move(par_ctx.semantic_tokens);
        // TODO: update identifier tokens with binary search during symbol resolution
    }

    return std::move(collector.symbol_order);
}
