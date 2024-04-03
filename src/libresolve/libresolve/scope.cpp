#include <libutl/common/utilities.hpp>
#include <libresolve/module.hpp>

namespace {
    constexpr auto first = [](auto& pair) noexcept -> auto& { return std::get<0>(pair); };

    template <class Binding>
    auto do_bind(
        utl::Flatmap<kieli::Identifier, Binding>& bindings,
        kieli::Identifier const                   identifier,
        Binding                                   binding) -> void
    {
        // Easy way to implement variable shadowing
        auto const it = std::ranges::find(bindings, identifier, first);
        bindings.container().emplace(it, identifier, std::move(binding));
    }

    template <auto libresolve::Scope::*bindings>
    auto do_find(libresolve::Scope* scope, kieli::Identifier const identifier)
        -> decltype((scope->*bindings).find(identifier))
    {
        for (; scope; scope = scope->parent()) {
            if (auto* const binding = (scope->*bindings).find(identifier)) {
                return binding;
            }
        }
        return nullptr;
    }

    template <class Binding>
    auto do_report_unused(
        kieli::Diagnostics&                             diagnostics,
        utl::Source::Wrapper const                      source,
        utl::Flatmap<kieli::Identifier, Binding> const& bindings)
    {
        auto const unused = std::views::values | std::views::filter(&Binding::unused);
        for (auto const& binding : unused(bindings)) {
            diagnostics.emit(
                cppdiag::Severity::warning,
                source,
                binding.name.source_range,
                "Unused binding: {}",
                binding.name);
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
    return do_find<&Scope::m_mutabilities>(this, identifier);
}

auto libresolve::Scope::find_variable(kieli::Identifier const identifier) -> Variable_bind*
{
    return do_find<&Scope::m_variables>(this, identifier);
}

auto libresolve::Scope::find_type(kieli::Identifier const identifier) -> Type_bind*
{
    return do_find<&Scope::m_types>(this, identifier);
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

auto libresolve::Scope::source() const noexcept -> utl::Source::Wrapper
{
    return m_source;
}

auto libresolve::Scope::report_unused(
    kieli::Diagnostics& diagnostics, utl::Source::Wrapper const source) -> void
{
    do_report_unused(diagnostics, source, m_types);
    do_report_unused(diagnostics, source, m_variables);
    do_report_unused(diagnostics, source, m_mutabilities);
}
