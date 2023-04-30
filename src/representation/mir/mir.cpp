#include "utl/utilities.hpp"
#include "mir.hpp"


namespace {
    auto flatten(utl::Wrapper<mir::Type::Variant> const type) -> void {
        while (auto const* const variable = std::get_if<mir::type::Unification_variable>(&*type)) {
            if (auto const* const solved = std::get_if<mir::Unification_type_variable_state::Solved>(&variable->state->value)) {
                *type = *solved->solution.pure_value();
            }
            else
                return;
        }
    }
    auto flatten(utl::Wrapper<mir::Mutability::Variant> const mutability) -> void {
        while (auto const* const variable = std::get_if<mir::Mutability::Variable>(&*mutability)) {
            if (auto const* const solved = std::get_if<mir::Unification_mutability_variable_state::Solved>(&variable->state->value))
                *mutability = *solved->solution.value();
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
    return flatten(m_value), m_value;
}
auto mir::Type::source_view() const noexcept -> utl::Source_view {
    return m_source_view;
}
auto mir::Type::with(utl::Source_view const source_view) const noexcept -> Type {
    return Type { m_value, source_view };
}

mir::Mutability::Mutability(utl::Wrapper<Variant> const value, utl::Source_view const source_view) noexcept
    : m_value { value }, m_source_view { source_view } {}

auto mir::Mutability::value() const -> utl::Wrapper<Variant> {
    return flatten(m_value), m_value;
}
auto mir::Mutability::source_view() const noexcept -> utl::Source_view {
    return m_source_view;
}
auto mir::Mutability::with(utl::Source_view const source_view) const noexcept -> Mutability {
    return Mutability { m_value, source_view };
}


auto mir::Unification_type_variable_state::solve(Type const solution) -> void {
    utl::always_assert(std::holds_alternative<Unsolved>(value));
    value = Solved { .solution = solution };
}
auto mir::Unification_type_variable_state::as_unsolved(std::source_location const caller) noexcept -> Unsolved& {
    return utl::get<Unsolved>(value, caller);
}
auto mir::Unification_type_variable_state::as_unsolved(std::source_location const caller) const noexcept -> Unsolved const& {
    return utl::get<Unsolved>(value, caller);
}

auto mir::Unification_mutability_variable_state::solve(Mutability const solution) -> void {
    utl::always_assert(std::holds_alternative<Unsolved>(value));
    value = Solved { .solution = solution };
}
auto mir::Unification_mutability_variable_state::as_unsolved(std::source_location const caller) noexcept -> Unsolved& {
    return utl::get<Unsolved>(value, caller);
}
auto mir::Unification_mutability_variable_state::as_unsolved(std::source_location const caller) const noexcept -> Unsolved const& {
    return utl::get<Unsolved>(value, caller);
}
