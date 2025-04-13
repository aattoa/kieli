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
} // namespace

auto ki::res::resolve_mutability(
    db::Database& db, Context& ctx, Scope_id scope_id, ast::Mutability const& mut)
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
            if (auto const* const bound = find_mutability(ctx, scope_id, mut.name.id)) {
                return bound->mutability;
            }

            db::add_error(
                db,
                ctx.scopes.index_vector[scope_id].doc_id,
                mut.name.range,
                std::format("No mutability '{}' in scope", db.string_pool.get(mut.name.id)));

            return hir::Mutability {
                .id    = ctx.constants.mut_error,
                .range = mut.name.range,
            };
        },
    };
    return std::visit(visitor, mut.variant);
}
