#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    enum class Unification_goal { equality, subtype };

    struct Type_unification_arguments {
        using Report_unification_failure = void(
            kieli::Diagnostics&       diagnostics,
            hir::Type::Variant const& sub,
            hir::Type::Variant const& super);
        using Report_recursive_solution = void(
            kieli::Diagnostics& diagnostics, Type_variable_tag tag, hir::Type::Variant const& type);
        Unification_goal            goal {};
        Report_unification_failure* report_unification_failure {};
        Report_recursive_solution*  report_recursive_solution {};
    };

    struct Mutability_unification_arguments {
        using Report_unification_failure
            = void(kieli::Diagnostics& diagnostics, hir::Mutability sub, hir::Mutability super);
        Unification_goal            goal {};
        Report_unification_failure* report_unification_failure {};
    };

    struct Mutability_unification_visitor {
        kieli::Diagnostics&                     diagnostics;
        Inference_state&                        state;
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
            state.set_solution(diagnostics, state.mutability_variables[tag], *mutability.variant);
            return true;
        }

        [[nodiscard]] auto unify(hir::Mutability const sub, hir::Mutability const super) const
            -> bool
        {
            Mutability_unification_arguments const recurse_arguments {
                arguments.goal,
                arguments.report_unification_failure,
            };
            bool const result = std::visit(
                Mutability_unification_visitor {
                    diagnostics, state, recurse_arguments, sub, super },
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
                state.mutability_variable_disjoint_set.merge(sub.tag.get(), super.tag.get());
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
        Inference_state&                  state;
        Type_unification_arguments const& arguments;
        hir::Type::Variant const&         current_sub;
        hir::Type::Variant const&         current_super;

        [[nodiscard]] auto allow_coercion() const noexcept -> bool
        {
            return arguments.goal == Unification_goal::subtype;
        }

        [[nodiscard]] auto solution(Type_variable_tag const tag, hir::Type::Variant type) const
            -> bool
        {
            if (occurs_check(tag, type)) {
                if (arguments.report_recursive_solution) {
                    arguments.report_recursive_solution(diagnostics, tag, type);
                }
                return false;
            }
            state.flatten(type);
            state.set_solution(diagnostics, state.type_variables[tag], std::move(type));
            return true;
        }

        [[nodiscard]] auto unify(
            hir::Type::Variant const& sub, hir::Type::Variant const& super) const -> bool
        {
            Type_unification_arguments const recurse_arguments {
                .goal                       = arguments.goal,
                .report_unification_failure = arguments.report_unification_failure,
                .report_recursive_solution  = arguments.report_recursive_solution,
            };
            Type_unification_visitor visitor { diagnostics, state, recurse_arguments, sub, super };
            bool const               result = std::visit(visitor, sub, super);
            if (!result && arguments.report_unification_failure) {
                arguments.report_unification_failure(diagnostics, sub, super);
            }
            return result;
        }

        [[nodiscard]] auto unify(hir::Type const sub, hir::Type const super) const -> bool
        {
            return unify(*sub.variant, *super.variant);
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
                .goal                       = arguments.goal,
                .report_unification_failure = nullptr,
            };
            Mutability_unification_visitor visitor {
                diagnostics, state, mutability_arguments, sub, super
            };
            return visitor.unify(sub, super);
        }

        auto operator()(hir::type::Variable const sub, hir::type::Variable const super) const
            -> bool
        {
            // TODO: handle integrals
            if (sub.tag != super.tag) {
                state.type_variable_disjoint_set.merge(sub.tag.get(), super.tag.get());
            }
            return true;
        }

        auto operator()(hir::type::Variable const sub, auto const&) const -> bool
        {
            // TODO: handle integrals
            return solution(sub.tag, current_super);
        }

        auto operator()(auto const&, hir::type::Variable const super) const -> bool
        {
            // TODO: handle integrals
            return solution(super.tag, current_sub);
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
        Inference_state&                  state,
        Type_unification_arguments const& arguments,
        hir::Type::Variant const&         sub,
        hir::Type::Variant const&         super) -> bool
    {
        Type_unification_visitor const visitor { diagnostics, state, arguments, sub, super };
        return visitor.unify(sub, super);
    }
} // namespace

auto libresolve::require_subtype_relationship(
    kieli::Diagnostics&       diagnostics,
    Inference_state&          state,
    hir::Type::Variant const& sub,
    hir::Type::Variant const& super) -> void
{
    Type_unification_arguments const arguments {
        .goal = Unification_goal::subtype,
    };

    if (!unify(diagnostics, state, arguments, sub, super)) {
        auto const sub_type_string   = hir::to_string(sub);
        auto const super_type_string = hir::to_string(super);

        diagnostics.error("Unable to unify {} ~ {}", sub_type_string, super_type_string);
    }
}
