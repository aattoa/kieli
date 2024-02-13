#include <libutl/common/utilities.hpp>
#include <libresolve/module.hpp>

auto libresolve::Scope::bind_mutability(
    kieli::Identifier const identifier,
    Mutability_bind         binding,
    kieli::Diagnostics&     diagnostics) -> void
{
    (void)this;
    (void)identifier;
    (void)binding;
    (void)diagnostics;
    cpputil::todo();
};

auto libresolve::Scope::bind_variable(
    kieli::Identifier const identifier,
    Variable_bind           binding,
    kieli::Diagnostics&     diagnostics) -> void
{
    (void)this;
    (void)identifier;
    (void)binding;
    (void)diagnostics;
    cpputil::todo();
};

auto libresolve::Scope::bind_type(
    kieli::Identifier const identifier, Type_bind binding, kieli::Diagnostics& diagnostics) -> void
{
    (void)this;
    (void)identifier;
    (void)binding;
    (void)diagnostics;
    cpputil::todo();
};

auto libresolve::Scope::find_mutability(kieli::Identifier const identifier) -> Mutability_bind*
{
    (void)this;
    (void)identifier;
    cpputil::todo();
}

auto libresolve::Scope::find_variable(kieli::Identifier const identifier) -> Variable_bind*
{
    (void)this;
    (void)identifier;
    cpputil::todo();
}

auto libresolve::Scope::find_type(kieli::Identifier const identifier) -> Type_bind*
{
    (void)this;
    (void)identifier;
    cpputil::todo();
}

auto libresolve::Scope::child() -> Scope
{
    Scope scope;
    scope.m_parent = this;
    return scope;
}
