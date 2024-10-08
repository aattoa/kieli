#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto resolve_concrete(Constants const& constants, kieli::Mutability const mutability)
    {
        switch (mutability) {
        case kieli::Mutability::mut:   return constants.mutability_yes;
        case kieli::Mutability::immut: return constants.mutability_no;
        default:                       cpputil::unreachable();
        }
    }

    auto error(Context& context, kieli::Document_id const document_id, kieli::Lower const name)
        -> hir::Mutability
    {
        auto message = std::format("No mutability binding '{}' in scope", name);
        kieli::add_error(context.db, document_id, name.range, std::move(message));
        return hir::Mutability { context.constants.mutability_error, name.range };
    }
} // namespace

auto libresolve::resolve_mutability(
    Context& context, Scope& scope, ast::Mutability const& mutability) -> hir::Mutability
{
    auto visitor = utl::Overload {
        [&](kieli::Mutability const& concrete) {
            return hir::Mutability {
                .id    = resolve_concrete(context.constants, concrete),
                .range = mutability.range,
            };
        },
        [&](ast::Parameterized_mutability const& parameterized) {
            auto const* const bound = scope.find_mutability(parameterized.name.identifier);
            return bound ? bound->mutability
                         : error(context, scope.document_id(), parameterized.name);
        },
    };
    return std::visit(visitor, mutability.variant);
}
