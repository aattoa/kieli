#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    auto resolve_concrete(Constants const& constants, ki::Mutability mut)
    {
        switch (mut) {
        case ki::Mutability::Mut:   return constants.mut_yes;
        case ki::Mutability::Immut: return constants.mut_no;
        default:                    cpputil::unreachable();
        }
    }

    auto mutability_error(Context& ctx, ki::Document_id doc_id, ki::Lower name) -> hir::Mutability
    {
        auto message = std::format("No mutability '{}' in scope", ctx.db.string_pool.get(name.id));
        ki::add_error(ctx.db, doc_id, name.range, std::move(message));
        return { .id = ctx.constants.mut_error, .range = name.range };
    }
} // namespace

auto ki::resolve::resolve_mutability(
    Context& ctx, hir::Scope_id const scope_id, ast::Mutability const& mut) -> hir::Mutability
{
    auto visitor = utl::Overload {
        [&](Mutability const& concrete) {
            return hir::Mutability {
                .id    = resolve_concrete(ctx.constants, concrete),
                .range = mut.range,
            };
        },
        [&](ast::Parameterized_mutability const& parameterized) {
            auto const* const bound  = find_mutability(ctx, scope_id, parameterized.name.id);
            auto const        doc_id = ctx.info.scopes.index_vector[scope_id].doc_id;
            return bound ? bound->mutability : mutability_error(ctx, doc_id, parameterized.name);
        },
    };
    return std::visit(visitor, mut.variant);
}
