#include <libutl/utilities.hpp>
#include <libresolve/module.hpp>

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
    auto do_find(Scope* scope, kieli::Identifier const identifier) -> T*
    {
        for (; scope; scope = scope->parent()) {
            auto const it = std::ranges::find(scope->*bindings, identifier, utl::first);
            if (it != (scope->*bindings).end()) {
                return std::addressof(it->second);
            }
        }
        return nullptr;
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

auto libresolve::Scope::bind_mutability(kieli::Identifier const identifier, Mutability_bind binding)
    -> void
{
    do_bind(m_mutabilities, identifier, std::move(binding));
};

auto libresolve::Scope::bind_variable(kieli::Identifier const identifier, Variable_bind binding)
    -> void
{
    do_bind(m_variables, identifier, std::move(binding));
};

auto libresolve::Scope::bind_type(kieli::Identifier const identifier, Type_bind binding) -> void
{
    do_bind(m_types, identifier, std::move(binding));
};

auto libresolve::Scope::find_mutability(kieli::Identifier const identifier) -> Mutability_bind*
{
    return do_find<Mutability_bind, &Scope::m_mutabilities>(this, identifier);
}

auto libresolve::Scope::find_variable(kieli::Identifier const identifier) -> Variable_bind*
{
    return do_find<Variable_bind, &Scope::m_variables>(this, identifier);
}

auto libresolve::Scope::find_type(kieli::Identifier const identifier) -> Type_bind*
{
    return do_find<Type_bind, &Scope::m_types>(this, identifier);
}

auto libresolve::Scope::child() noexcept -> Scope
{
    Scope scope { m_document_id };
    scope.m_parent = this;
    return scope;
}

auto libresolve::Scope::parent() const noexcept -> Scope*
{
    return m_parent;
}

auto libresolve::Scope::document_id() const noexcept -> kieli::Document_id
{
    return m_document_id;
}

auto libresolve::Scope::report_unused(kieli::Database& db) -> void
{
    kieli::Document& document = db.documents.at(document_id());
    do_report_unused_bindings(document, m_types);
    do_report_unused_bindings(document, m_variables);
    do_report_unused_bindings(document, m_mutabilities);
}
