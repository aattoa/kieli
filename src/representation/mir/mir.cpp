#include "utl/utilities.hpp"
#include "mir.hpp"


namespace {
    template <class Variable>
    auto flatten(utl::wrapper auto const wrapper) -> void {
        while (Variable const* const variable = std::get_if<Variable>(&*wrapper)) {
            if (auto const* const solved = variable->state->as_solved_if())
                *wrapper = *solved->solution.pure_value();
            else
                return;
        }
    }
}


mir::Type::Type(utl::Wrapper<Variant> const value, utl::Source_view const source_view) noexcept
    : m_value { value }, m_source_view { source_view } {}

auto mir::Type::pure_value() const noexcept -> utl::Wrapper<Variant> {
    return m_value;
}
auto mir::Type::flattened_value() const -> utl::Wrapper<Variant> {
    flatten<type::Unification_variable>(m_value);
    return m_value;
}
auto mir::Type::source_view() const noexcept -> utl::Source_view {
    return m_source_view;
}
auto mir::Type::with(utl::Source_view const source_view) const noexcept -> Type {
    return Type { m_value, source_view };
}

mir::Mutability::Mutability(utl::Wrapper<Variant> const value, utl::Source_view const source_view) noexcept
    : m_value { value }, m_source_view { source_view } {}

auto mir::Mutability::pure_value() const noexcept -> utl::Wrapper<Variant> {
    return m_value;
}
auto mir::Mutability::flattened_value() const -> utl::Wrapper<Variant> {
    flatten<Variable>(m_value);
    return m_value;
}
auto mir::Mutability::source_view() const noexcept -> utl::Source_view {
    return m_source_view;
}
auto mir::Mutability::with(utl::Source_view const source_view) const noexcept -> Mutability {
    return Mutability { m_value, source_view };
}


mir::Unification_type_variable_state::Unification_type_variable_state(Unsolved&& unsolved) noexcept
    : m_value { std::move(unsolved) } {}

auto mir::Unification_type_variable_state::solve(Type const solution) -> void {
    utl::always_assert(std::holds_alternative<Unsolved>(m_value));
    m_value = Solved { .solution = solution };
}
auto mir::Unification_type_variable_state::as_unsolved(std::source_location const caller) noexcept -> Unsolved& {
    return utl::get<Unsolved>(m_value, caller);
}
auto mir::Unification_type_variable_state::as_unsolved(std::source_location const caller) const noexcept -> Unsolved const& {
    return utl::get<Unsolved>(m_value, caller);
}
auto mir::Unification_type_variable_state::as_solved_if() noexcept -> Solved* {
    return std::get_if<Solved>(&m_value);
}
auto mir::Unification_type_variable_state::as_solved_if() const noexcept -> Solved const* {
    return std::get_if<Solved>(&m_value);
}

mir::Unification_mutability_variable_state::Unification_mutability_variable_state(Unsolved&& unsolved) noexcept
    : m_value { std::move(unsolved) } {}

auto mir::Unification_mutability_variable_state::solve(Mutability const solution) -> void {
    utl::always_assert(std::holds_alternative<Unsolved>(m_value));
    m_value = Solved { .solution = solution };
}
auto mir::Unification_mutability_variable_state::as_unsolved(std::source_location const caller) noexcept -> Unsolved& {
    return utl::get<Unsolved>(m_value, caller);
}
auto mir::Unification_mutability_variable_state::as_unsolved(std::source_location const caller) const noexcept -> Unsolved const& {
    return utl::get<Unsolved>(m_value, caller);
}
auto mir::Unification_mutability_variable_state::as_solved_if() noexcept -> Solved* {
    return std::get_if<Solved>(&m_value);
}
auto mir::Unification_mutability_variable_state::as_solved_if() const noexcept -> Solved const* {
    return std::get_if<Solved>(&m_value);
}
