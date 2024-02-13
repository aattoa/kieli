#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <libresolve/unification.hpp>

auto libresolve::Unification_state::fresh_general_type_variable()
    -> utl::Mutable_wrapper<hir::Unification_type_variable_state>
{
    return m_state_arena.wrap_mutable<hir::Unification_type_variable_state>(
        hir::Unification_type_variable_state::Unsolved {
            .tag  = fresh_tag(),
            .kind = hir::Unification_type_variable_kind::general,
        });
};

auto libresolve::Unification_state::fresh_integral_type_variable()
    -> utl::Mutable_wrapper<hir::Unification_type_variable_state>
{
    return m_state_arena.wrap_mutable<hir::Unification_type_variable_state>(
        hir::Unification_type_variable_state::Unsolved {
            .tag  = fresh_tag(),
            .kind = hir::Unification_type_variable_kind::integral,
        });
};

auto libresolve::Unification_state::fresh_mutability_variable()
    -> utl::Mutable_wrapper<hir::Unification_mutability_variable_state>
{
    return m_state_arena.wrap_mutable<hir::Unification_mutability_variable_state>(
        hir::Unification_mutability_variable_state::Unsolved {
            .tag = fresh_tag(),
        });
};

auto libresolve::Unification_state::fresh_tag() -> hir::Unification_variable_tag
{
    cpputil::always_assert(!utl::would_increment_overflow(m_current_variable_tag));
    return hir::Unification_variable_tag { m_current_variable_tag++ };
};
