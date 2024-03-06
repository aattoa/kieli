#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    enum class Unification_goal { equality, subtype };

    struct Type_unification_arguments {
        using Report_unification_failure
            = void(kieli::Diagnostics& diagnostics, hir::Type sub, hir::Type super);
        using Report_recursive_solution
            = void(kieli::Diagnostics& diagnostics, Type_variable_tag tag, hir::Type type);
        utl::Source_range           origin;
        Unification_goal            goal {};
        Report_unification_failure* report_unification_failure {};
        Report_recursive_solution*  report_recursive_solution {};
    };

    struct Mutability_unification_arguments {
        using Report_unification_failure
            = void(kieli::Diagnostics& diagnostics, hir::Mutability sub, hir::Mutability super);
        utl::Source_range           origin;
        Unification_goal            goal {};
        Report_unification_failure* report_unification_failure {};
    };

    struct Mutability_unification_visitor {
        kieli::Diagnostics&                     diagnostics;
        Unification_solutions&                  solutions;
        Mutability_unification_arguments const& arguments;
        hir::Mutability const&                  current_sub;
        hir::Mutability const&                  current_super;

        [[nodiscard]] auto allow_coercion() const noexcept -> bool
        {
            return arguments.goal == Unification_goal::subtype;
        }

        [[nodiscard]] auto solution(
            Mutability_variable_tag const tag, hir::Mutability const mutability) const -> bool
        {
            if (auto const* const previous_solution = solutions.mutability_map.find(tag)) {
                // TODO: Should coercion be disallowed here?
                return unify(mutability, *previous_solution);
            }
            solutions.mutability_map.add_new_unchecked(tag, mutability);
            return true;
        }

        [[nodiscard]] auto unify(hir::Mutability const sub, hir::Mutability const super) const
            -> bool
        {
            Mutability_unification_arguments const recurse_arguments {
                arguments.origin,
                arguments.goal,
                arguments.report_unification_failure,
            };
            bool const result = std::visit(
                Mutability_unification_visitor {
                    diagnostics, solutions, recurse_arguments, sub, super },
                *sub.variant,
                *super.variant);
            if (!result && arguments.report_unification_failure) {
                arguments.report_unification_failure(diagnostics, sub, super);
            }
            return result;
        }

        auto operator()(hir::mutability::Concrete const sub, hir::mutability::Concrete const super)
            const -> bool
        {
            return (sub == super) || (sub == hir::mutability::Concrete::mut && allow_coercion());
        }

        auto operator()(hir::mutability::Variable const sub, hir::mutability::Variable const super)
            const -> bool
        {
            if (sub.tag != super.tag) {
                solutions.mutability_variable_equalities[sub.tag].push_back(super.tag);
                solutions.mutability_variable_equalities[super.tag].push_back(sub.tag);
            }
            return true;
        }

        auto operator()(hir::mutability::Variable const variable, auto const&) const -> bool
        {
            return solution(variable.tag, current_super);
        }

        auto operator()(auto const&, hir::mutability::Variable const variable) const -> bool
        {
            return solution(variable.tag, current_sub);
        }

        auto operator()(
            hir::mutability::Parameterized const sub,
            hir::mutability::Parameterized const super) const -> bool
        {
            return sub.tag == super.tag;
        }

        auto operator()(auto const&, auto const&) const -> bool
        {
            return false;
        }
    };

    struct Type_unification_visitor {
        kieli::Diagnostics&               diagnostics;
        Unification_solutions&            solutions;
        Type_unification_arguments const& arguments;
        hir::Type const&                  current_sub;
        hir::Type const&                  current_super;

        [[nodiscard]] auto allow_coercion() const noexcept -> bool
        {
            return arguments.goal == Unification_goal::subtype;
        }

        [[nodiscard]] auto solution(Type_variable_tag const tag, hir::Type const type) const -> bool
        {
            if (occurs_check(tag, type)) {
                if (arguments.report_recursive_solution) {
                    arguments.report_recursive_solution(diagnostics, tag, type);
                }
                return false;
            }
            if (auto const* const previous_solution = solutions.type_map.find(tag)) {
                // TODO: Should coercion be disallowed here?
                return unify(type, *previous_solution);
            }
            solutions.type_map.add_new_unchecked(tag, type);
            return true;
        }

        [[nodiscard]] auto unify(hir::Type const sub, hir::Type const super) const -> bool
        {
            Type_unification_arguments const recurse_arguments {
                .origin                     = arguments.origin,
                .goal                       = arguments.goal,
                .report_unification_failure = arguments.report_unification_failure,
                .report_recursive_solution  = arguments.report_recursive_solution,
            };
            bool const result = std::visit(
                Type_unification_visitor { diagnostics, solutions, recurse_arguments, sub, super },
                *sub.variant,
                *super.variant);
            if (!result && arguments.report_unification_failure) {
                arguments.report_unification_failure(diagnostics, sub, super);
            }
            return result;
        }

        [[nodiscard]] auto unify() const
        {
            return utl::Overload {
                [&](hir::Type const sub, hir::Type const super) -> bool {
                    return unify(sub, super);
                },
                [&](auto const& pair) -> bool {
                    return unify(std::get<0>(pair), std::get<1>(pair));
                },
            };
        }

        [[nodiscard]] auto unify(hir::Mutability const sub, hir::Mutability const super) const
            -> bool
        {
            Mutability_unification_arguments const mutability_arguments {
                .origin                     = arguments.origin,
                .goal                       = arguments.goal,
                .report_unification_failure = nullptr,
            };
            Mutability_unification_visitor visitor {
                diagnostics, solutions, mutability_arguments, sub, super
            };
            return visitor.unify(sub, super);
        }

        auto operator()(hir::type::Variable const sub, hir::type::Variable const super) const
            -> bool
        {
            // TODO: handle integrals
            if (sub.tag != super.tag) {
                solutions.type_variable_equalities[sub.tag].push_back(super.tag);
                solutions.type_variable_equalities[super.tag].push_back(sub.tag);
            }
            return true;
        }

        auto operator()(hir::type::Variable const variable, auto const&) const -> bool
        {
            // TODO: handle integrals
            return solution(variable.tag, current_super);
        }

        auto operator()(auto const&, hir::type::Variable const variable) const -> bool
        {
            // TODO: handle integrals
            return solution(variable.tag, current_sub);
        }

        template <utl::one_of<
            kieli::built_in_type::Floating,
            kieli::built_in_type::Character,
            kieli::built_in_type::Boolean,
            kieli::built_in_type::String> T>
        auto operator()(T, T) const -> bool
        {
            return true;
        }

        auto operator()(
            kieli::built_in_type::Integer const sub,
            kieli::built_in_type::Integer const super) const -> bool
        {
            return sub == super;
        }

        auto operator()(hir::type::Parameterized const& sub, hir::type::Parameterized const& super)
            const -> bool
        {
            return sub.tag == super.tag;
        }

        auto operator()(hir::type::Tuple const& sub, hir::type::Tuple const& super) const -> bool
        {
            return sub.types.size() == super.types.size()
                && std::ranges::all_of(std::views::zip(sub.types, super.types), unify());
        }

        auto operator()(hir::type::Array const& sub, hir::type::Array const& super) const -> bool
        {
            return unify(sub.element_type, super.element_type)
                && unify(sub.length->type, super.length->type);
        }

        auto operator()(hir::type::Slice const& sub, hir::type::Slice const& super) const -> bool
        {
            return unify(sub.element_type, super.element_type);
        }

        auto operator()(hir::type::Reference const& sub, hir::type::Reference const& super) const
            -> bool
        {
            return unify(sub.referenced_type, super.referenced_type)
                && unify(sub.mutability, super.mutability);
        }

        auto operator()(hir::type::Pointer const& sub, hir::type::Pointer const& super) const
            -> bool
        {
            return unify(sub.pointee_type, super.pointee_type)
                && unify(sub.mutability, super.mutability);
        }

        auto operator()(hir::type::Function const& sub, hir::type::Function const& super) const
            -> bool
        {
            return sub.parameter_types.size() == super.parameter_types.size()
                && unify(sub.return_type, super.return_type)
                && std::ranges::all_of(
                       std::views::zip(sub.parameter_types, super.parameter_types), unify());
        }

        auto operator()(
            hir::type::Enumeration const& sub, hir::type::Enumeration const& super) const -> bool
        {
            // TODO: visit template arguments
            return sub.info.is(super.info);
        }

        auto operator()(hir::type::Error, hir::type::Error) const -> bool
        {
            return true;
        }

        template <class T>
            requires(!std::is_same_v<T, hir::type::Variable>)
        auto operator()(hir::type::Error, T const&) const -> bool
        {
            return true;
        }

        template <class T>
            requires(!std::is_same_v<T, hir::type::Variable>)
        auto operator()(T const&, hir::type::Error) const -> bool
        {
            return true;
        }

        auto operator()(auto const&, auto const&) const -> bool
        {
            return false;
        }
    };

    auto unify(
        kieli::Diagnostics&               diagnostics,
        Type_unification_arguments const& arguments,
        hir::Type const                   sub,
        hir::Type const                   super) -> std::optional<Unification_solutions>
    {
        Unification_solutions          solutions;
        Type_unification_visitor const visitor { diagnostics, solutions, arguments, sub, super };
        bool const                     result = visitor.unify(sub, super);
        return result ? std::optional(std::move(solutions)) : std::nullopt;
    }

    auto apply_solutions(Inference_state& state, Unification_solutions const& solutions) -> void
    {
        for (auto const& [tag, solution] : solutions.type_map) {
            state.type_variables[tag].solve_with(*solution.variant);
            for (auto const equal_tag : solutions.type_variable_equalities[tag]) {
                Type_variable_data& equal = state.type_variables[equal_tag];
                cpputil::always_assert(!equal.is_solved); // TODO
                equal.solve_with(*solution.variant);
            }
        }
    }
} // namespace

auto libresolve::require_subtype_relationship(
    kieli::Diagnostics&     diagnostics,
    Inference_state&        state,
    hir::Type const         sub,
    hir::Type const         super,
    utl::Source_range const origin) -> void
{
    Type_unification_arguments const arguments {
        .origin = origin,
        .goal   = Unification_goal::subtype,
    };
    if (auto solutions = unify(diagnostics, arguments, sub, super)) {
        apply_solutions(state, solutions.value());
    }
    else {
        // TODO: fix message
        diagnostics.error(
            state.source,
            origin,
            "Unable to unify {} <: {}",
            sub.variant->index(),
            super.variant->index());
    }
}
