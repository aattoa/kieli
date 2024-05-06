#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    auto resolve_concrete(
        libresolve::Constants const& constants, ast::mutability::Concrete const concrete)
    {
        switch (concrete) {
        case ast::mutability::Concrete::mut:
            return constants.mutability_yes;
        case ast::mutability::Concrete::immut:
            return constants.mutability_no;
        default:
            cpputil::unreachable();
        }
    }
} // namespace

auto libresolve::resolve_mutability(
    Context& context, Scope& scope, ast::Mutability const& mutability) -> hir::Mutability
{
    return std::visit(
        utl::Overload {
            [&](ast::mutability::Concrete const& concrete) {
                return hir::Mutability {
                    .variant      = resolve_concrete(context.constants, concrete),
                    .source_range = mutability.source_range,
                };
            },
            [&](ast::mutability::Parameterized const& parameterized) {
                if (auto const* const bound = scope.find_mutability(parameterized.name.identifier))
                {
                    return bound->mutability;
                }
                context.compile_info.diagnostics.emit(
                    cppdiag::Severity::error,
                    scope.source(),
                    parameterized.name.source_range,
                    "No mutability binding '{}' in scope",
                    parameterized.name);
                return hir::Mutability {
                    .variant      = context.constants.mutability_error,
                    .source_range = mutability.source_range,
                };
            },
        },
        mutability.variant);
}
