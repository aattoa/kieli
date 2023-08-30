#include <libutl/common/utilities.hpp>
#include <libresolve/hir.hpp>

namespace {
    template <class Variable>
    auto flatten(utl::wrapper auto const wrapper) -> void
    {
        while (Variable const* const variable = std::get_if<Variable>(&*wrapper)) {
            if (auto const* const solved = variable->state->as_solved_if()) {
                *wrapper = *solved->solution.pure_value();
            }
            else {
                return;
            }
        }
    }
} // namespace

hir::Type::Type(utl::Wrapper<Variant> const value, utl::Source_view const source_view) noexcept
    : m_value { value }
    , m_source_view { source_view }
{}

auto hir::Type::pure_value() const noexcept -> utl::Wrapper<Variant>
{
    return m_value;
}

auto hir::Type::flattened_value() const -> utl::Wrapper<Variant>
{
    flatten<type::Unification_variable>(m_value);
    return m_value;
}

auto hir::Type::source_view() const noexcept -> utl::Source_view
{
    return m_source_view;
}

auto hir::Type::with(utl::Source_view const source_view) const noexcept -> Type
{
    return Type { m_value, source_view };
}

hir::Mutability::Mutability(
    utl::Wrapper<Variant> const value, utl::Source_view const source_view) noexcept
    : m_value { value }
    , m_source_view { source_view }
{}

auto hir::Mutability::pure_value() const noexcept -> utl::Wrapper<Variant>
{
    return m_value;
}

auto hir::Mutability::flattened_value() const -> utl::Wrapper<Variant>
{
    flatten<Variable>(m_value);
    return m_value;
}

auto hir::Mutability::source_view() const noexcept -> utl::Source_view
{
    return m_source_view;
}

auto hir::Mutability::with(utl::Source_view const source_view) const noexcept -> Mutability
{
    return Mutability { m_value, source_view };
}

hir::Unification_type_variable_state::Unification_type_variable_state(Unsolved&& unsolved) noexcept
    : m_value { std::move(unsolved) }
{}

auto hir::Unification_type_variable_state::solve_with(Type const solution) -> void
{
    utl::always_assert(std::holds_alternative<Unsolved>(m_value));
    m_value = Solved { .solution = solution };
}

auto hir::Unification_type_variable_state::as_unsolved(std::source_location const caller) noexcept
    -> Unsolved&
{
    return utl::get<Unsolved>(m_value, caller);
}

auto hir::Unification_type_variable_state::as_unsolved(
    std::source_location const caller) const noexcept -> Unsolved const&
{
    return utl::get<Unsolved>(m_value, caller);
}

auto hir::Unification_type_variable_state::as_solved_if() noexcept -> Solved*
{
    return std::get_if<Solved>(&m_value);
}

auto hir::Unification_type_variable_state::as_solved_if() const noexcept -> Solved const*
{
    return std::get_if<Solved>(&m_value);
}

hir::Unification_mutability_variable_state::Unification_mutability_variable_state(
    Unsolved&& unsolved) noexcept
    : m_value { std::move(unsolved) }
{}

auto hir::Unification_mutability_variable_state::solve_with(Mutability const solution) -> void
{
    utl::always_assert(std::holds_alternative<Unsolved>(m_value));
    m_value = Solved { .solution = solution };
}

auto hir::Unification_mutability_variable_state::as_unsolved(
    std::source_location const caller) noexcept -> Unsolved&
{
    return utl::get<Unsolved>(m_value, caller);
}

auto hir::Unification_mutability_variable_state::as_unsolved(
    std::source_location const caller) const noexcept -> Unsolved const&
{
    return utl::get<Unsolved>(m_value, caller);
}

auto hir::Unification_mutability_variable_state::as_solved_if() noexcept -> Solved*
{
    return std::get_if<Solved>(&m_value);
}

auto hir::Unification_mutability_variable_state::as_solved_if() const noexcept -> Solved const*
{
    return std::get_if<Solved>(&m_value);
}

auto hir::Template_parameter::is_implicit() const noexcept -> bool
{
    auto const* const type_parameter = std::get_if<Type_parameter>(&value);
    return type_parameter && !type_parameter->name.has_value();
}

auto hir::Function::Signature::is_template() const noexcept -> bool
{
    return !template_parameters.empty();
}
