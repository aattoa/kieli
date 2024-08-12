#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto resolve_concrete(Constants const& constants, ast::mutability::Concrete const concrete)
    {
        switch (concrete) {
        case ast::mutability::Concrete::mut:   return constants.mutability_yes;
        case ast::mutability::Concrete::immut: return constants.mutability_no;
        default:                               cpputil::unreachable();
        }
    }

    auto error(Context& context, kieli::Document_id const document_id, kieli::Lower const name)
        -> hir::Mutability
    {
        kieli::Diagnostic diagnostic {
            .message  = std::format("No mutability binding '{}' in scope", name),
            .range    = name.range,
            .severity = kieli::Severity::error,
        };
        kieli::add_diagnostic(context.db, document_id, std::move(diagnostic));
        return hir::Mutability { context.constants.mutability_error, name.range };
    }
} // namespace

auto libresolve::resolve_mutability(
    Context& context, Scope& scope, ast::Mutability const& mutability) -> hir::Mutability
{
    return std::visit(
        utl::Overload {
            [&](ast::mutability::Concrete const& concrete) {
                return hir::Mutability {
                    .id    = resolve_concrete(context.constants, concrete),
                    .range = mutability.range,
                };
            },
            [&](ast::mutability::Parameterized const& parameterized) {
                auto const* const bound = scope.find_mutability(parameterized.name.identifier);
                return bound ? bound->mutability
                             : error(context, scope.document_id(), parameterized.name);
            },
        },
        mutability.variant);
}
