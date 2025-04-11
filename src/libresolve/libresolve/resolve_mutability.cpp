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
        default:                    cpputil::unreachable();
        }
    }

    auto mutability_error(Context& ctx, db::Document_id doc_id, db::Lower name) -> hir::Mutability
    {
        auto message = std::format("No mutability '{}' in scope", ctx.db.string_pool.get(name.id));
        db::add_error(ctx.db, doc_id, name.range, std::move(message));
        return hir::Mutability { .id = ctx.constants.mut_error, .range = name.range };
    }
} // namespace

auto ki::res::resolve_mutability(Context& ctx, Scope_id const scope_id, ast::Mutability const& mut)
    -> hir::Mutability
{
    auto const visitor = utl::Overload {
        [&](db::Mutability const& concrete) {
            return hir::Mutability {
                .id    = resolve_concrete(ctx.constants, concrete),
                .range = mut.range,
            };
        },
        [&](ast::Parameterized_mutability const& parameterized) {
            auto const* const bound  = find_mutability(ctx, scope_id, parameterized.name.id);
            auto const        doc_id = ctx.scopes.index_vector[scope_id].doc_id;
            return bound ? bound->mutability : mutability_error(ctx, doc_id, parameterized.name);
        },
    };
    return std::visit(visitor, mut.variant);
}
