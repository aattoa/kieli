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

    template <class Binding>
    auto do_report_unused(
        kieli::Database&               db,
        kieli::Source_id const         source,
        Identifier_map<Binding> const& bindings) -> void
    {
        auto const unused = std::views::values | std::views::filter(&Binding::unused);
        for (auto const& binding : unused(bindings)) {
            db.diagnostics.push_back(cppdiag::Diagnostic {
                .text_sections = utl::to_vector({
                    kieli::text_section(db.sources[source], binding.name.range),
                }),
                .message       = std::format("Unused binding: {}", binding.name),
                .help_note     = std::format("If this is intentional, use _{}", binding.name),
                .severity      = cppdiag::Severity::warning,
            });
        }
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
    Scope scope { m_source };
    scope.m_parent = this;
    return scope;
}

auto libresolve::Scope::parent() const noexcept -> Scope*
{
    return m_parent;
}

auto libresolve::Scope::source() const noexcept -> kieli::Source_id
{
    return m_source;
}

auto libresolve::Scope::report_unused(kieli::Database& db) -> void
{
    do_report_unused(db, m_source, m_types);
    do_report_unused(db, m_source, m_variables);
    do_report_unused(db, m_source, m_mutabilities);
}
