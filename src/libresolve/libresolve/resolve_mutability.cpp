#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto resolve_concrete(Constants const& constants, db::Mutability mut)
    {
        switch (mut) {
        case db::Mutability::Mut:   return constants.mut_yes;
        case db::Mutability::Immut: return constants.mut_no;
        }
        cpputil::unreachable();
    }

    auto make_path(db::Name name) -> ast::Path
    {
        ast::Path_segment segment { .template_arguments = std::nullopt, .name = name };
        return ast::Path { .root = {}, .segments = utl::to_vector({ std::move(segment) }) };
    }
} // namespace

auto ki::res::resolve_mutability(
    db::Database& db, Context& ctx, db::Environment_id env_id, ast::Mutability const& mut)
    -> hir::Mutability
{
    auto const visitor = utl::Overload {
        [&](db::Mutability const& concrete) {
            return hir::Mutability {
                .id    = resolve_concrete(ctx.constants, concrete),
                .range = mut.range,
            };
        },
        [&](ast::Parameterized_mutability const& mut) {
            auto          state     = Inference_state {};
            db::Symbol_id symbol_id = resolve_path(db, ctx, state, env_id, make_path(mut.name));
            db::Symbol&   symbol    = ctx.arena.symbols[symbol_id];

            if (auto const* local_id = std::get_if<hir::Local_mutability_id>(&symbol.variant)) {
                return hir::Mutability {
                    .id    = ctx.arena.hir.local_mutabilities[*local_id].mut_id,
                    .range = mut.name.range,
                };
            }
            if (not std::holds_alternative<db::Error>(symbol.variant)) {
                auto message = std::format(
                    "Expected a mutability binding, but '{}' is {}",
                    db.string_pool.get(mut.name.id),
                    db::describe_symbol_kind(symbol.variant));
                db::add_error(db, ctx.doc_id, mut.name.range, std::move(message));
            }
            return hir::Mutability {
                .id    = ctx.constants.mut_error,
                .range = mut.name.range,
            };
        },
    };
    return std::visit(visitor, mut.variant);
}
