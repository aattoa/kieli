#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    template <class Binding>
    auto do_bind(
        Identifier_map<Binding>& bindings,
        kieli::Identifier const  identifier,
        Binding                  binding) -> void
    {
        // Easy way to implement variable shadowing
        auto const it = std::ranges::find(bindings, identifier, utl::first);
        bindings.emplace(it, identifier, std::move(binding));
    }

    template <class T, Identifier_map<T> Scope::*bindings>
    auto do_find(Context& context, hir::Scope_id scope_id, kieli::Identifier const identifier) -> T*
    {
        for (;;) {
            Scope& scope = context.info.scopes.index_vector[scope_id];
            if (auto const it = std::ranges::find(scope.*bindings, identifier, utl::first);
                it != (scope.*bindings).end()) {
                return std::addressof(it->second);
            }
            else if (scope.parent_id.has_value()) {
                scope_id = scope.parent_id.value();
            }
            else {
                return nullptr;
            }
        }
    }

    auto warn_unused(kieli::Name const& name) -> kieli::Diagnostic
    {
        return kieli::Diagnostic {
            .message  = std::format("Unused name: {}. If this is intentional, use _{}", name, name),
            .range    = name.range,
            .severity = kieli::Severity::warning,
        };
    }

    template <class Binding>
    auto do_report_unused_bindings(
        kieli::Document& document, Identifier_map<Binding> const& bindings) -> void
    {
        auto diagnostics = std::views::values(bindings) //
                         | std::views::filter(&Binding::unused)
                         | std::views::transform(&Binding::name)
                         | std::views::transform(warn_unused);
        document.diagnostics.append_range(std::move(diagnostics));
    }
} // namespace

auto libresolve::bind_mutability(
    Scope& scope, kieli::Identifier const identifier, Mutability_bind bind) -> void
{
    do_bind(scope.mutabilities, identifier, std::move(bind));
}

auto libresolve::bind_variable(
    Scope& scope, kieli::Identifier const identifier, Variable_bind binding) -> void
{
    do_bind(scope.variables, identifier, std::move(binding));
}

auto libresolve::bind_type(Scope& scope, kieli::Identifier const identifier, Type_bind binding)
    -> void
{
    do_bind(scope.types, identifier, std::move(binding));
}

auto libresolve::find_mutability(
    Context&                context,
    hir::Scope_id const     scope_id,
    kieli::Identifier const identifier) -> Mutability_bind*
{
    return do_find<Mutability_bind, &Scope::mutabilities>(context, scope_id, identifier);
}

auto libresolve::find_variable(
    Context&                context,
    hir::Scope_id const     scope_id,
    kieli::Identifier const identifier) -> Variable_bind*
{
    return do_find<Variable_bind, &Scope::variables>(context, scope_id, identifier);
}

auto libresolve::find_type(
    Context&                context,
    hir::Scope_id const     scope_id,
    kieli::Identifier const identifier) -> Type_bind*
{
    return do_find<Type_bind, &Scope::types>(context, scope_id, identifier);
}

auto libresolve::report_unused(kieli::Database& db, Scope& scope) -> void
{
    kieli::Document& document = db.documents.at(scope.document_id);
    do_report_unused_bindings(document, scope.types);
    do_report_unused_bindings(document, scope.variables);
    do_report_unused_bindings(document, scope.mutabilities);
}
