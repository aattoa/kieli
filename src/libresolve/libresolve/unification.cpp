#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    enum struct Unification_goal { equality, subtype };

    struct Type_unification_arguments {
        using Report_unification_failure
            = void(Context& context, hir::Type_variant const& sub, hir::Type_variant const& super);
        using Report_recursive_solution
            = void(Context& context, hir::Type_variable_id id, hir::Type_variant const& type);
        Unification_goal            goal {};
        Report_unification_failure* report_unification_failure {};
        Report_recursive_solution*  report_recursive_solution {};
    };

    struct Mutability_unification_arguments {
        using Report_unification_failure
            = void(Context& context, hir::Mutability_id sub, hir::Mutability_id super);
        Unification_goal            goal {};
        Report_unification_failure* report_unification_failure {};
    };

    struct Mutability_unification_visitor {
        Context&                                context;
        Inference_state&                        state;
        Mutability_unification_arguments const& arguments;
        hir::Mutability const&                  current_sub;
        hir::Mutability const&                  current_super;

        [[nodiscard]] auto allow_coercion() const noexcept -> bool
        {
            return arguments.goal == Unification_goal::subtype;
        }

        [[nodiscard]] auto solution(
            hir::Mutability_variable_id const variable_id,
            hir::Mutability const             mutability) const -> bool
        {
            set_solution(
                context,
                state,
                state.mutability_variables[variable_id],
                context.hir.mutabilities[mutability.id]);
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
                Mutability_unification_visitor { context, state, recurse_arguments, sub, super },
                context.hir.mutabilities[sub.id],
                context.hir.mutabilities[super.id]);
            if (not result and arguments.report_unification_failure) {
                arguments.report_unification_failure(context, sub.id, super.id);
            }
            return result;
        }

        auto operator()(kieli::Mutability const sub, kieli::Mutability const super) const -> bool
        {
            return (sub == super) or (sub == kieli::Mutability::mut and allow_coercion());
        }

        auto operator()(hir::mutability::Variable const sub, hir::mutability::Variable const super)
            const -> bool
        {
            if (sub.id != super.id) {
                state.mutability_variable_disjoint_set.merge(sub.id.get(), super.id.get());
            }
            return true;
        }

        auto operator()(hir::mutability::Variable const variable, auto const&) const -> bool
        {
            return solution(variable.id, current_super);
        }

        auto operator()(auto const&, hir::mutability::Variable const variable) const -> bool
        {
            return solution(variable.id, current_sub);
        }

        auto operator()(
            hir::mutability::Parameterized const sub,
            hir::mutability::Parameterized const super) const -> bool
        {
            return sub.tag.get() == super.tag.get();
        }

        auto operator()(auto const&, auto const&) const -> bool
        {
            return false;
        }
    };

    struct Type_unification_visitor {
        Context&                          context;
        Inference_state&                  state;
        Type_unification_arguments const& arguments;
        hir::Type_variant const&          current_sub;
        hir::Type_variant const&          current_super;

        [[nodiscard]] auto allow_coercion() const noexcept -> bool
        {
            return arguments.goal == Unification_goal::subtype;
        }

        [[nodiscard]] auto solution(hir::Type_variable_id const tag, hir::Type_variant type) const
            -> bool
        {
            if (occurs_check(context.hir, tag, type)) {
                if (arguments.report_recursive_solution) {
                    arguments.report_recursive_solution(context, tag, type);
                }
                return false;
            }
            flatten_type(context, state, type);
            set_solution(context, state, state.type_variables[tag], std::move(type));
            return true;
        }

        [[nodiscard]] auto unify(hir::Type_variant const& sub, hir::Type_variant const& super) const
            -> bool
        {
            Type_unification_arguments const recurse_arguments {
                .goal                       = arguments.goal,
                .report_unification_failure = arguments.report_unification_failure,
                .report_recursive_solution  = arguments.report_recursive_solution,
            };
            Type_unification_visitor visitor { context, state, recurse_arguments, sub, super };
            bool const               result = std::visit(visitor, sub, super);
            if (not result and arguments.report_unification_failure) {
                arguments.report_unification_failure(context, sub, super);
            }
            return result;
        }

        [[nodiscard]] auto unify(hir::Type const sub, hir::Type const super) const -> bool
        {
            return unify(context.hir.types[sub.id], context.hir.types[super.id]);
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
                context, state, mutability_arguments, sub, super
            };
            return visitor.unify(sub, super);
        }

        auto operator()(hir::type::Variable const sub, hir::type::Variable const super) const
            -> bool
        {
            // TODO: handle integrals
            if (sub.id != super.id) {
                state.type_variable_disjoint_set.merge(sub.id.get(), super.id.get());
            }
            return true;
        }

        auto operator()(hir::type::Variable const sub, auto const&) const -> bool
        {
            // TODO: handle integrals
            return solution(sub.id, current_super);
        }

        auto operator()(auto const&, hir::type::Variable const super) const -> bool
        {
            // TODO: handle integrals
            return solution(super.id, current_sub);
        }

        template <utl::one_of<
            hir::type::Floating,
            hir::type::Character,
            hir::type::Boolean,
            hir::type::String> T>
        auto operator()(T, T) const -> bool
        {
            return true;
        }

        auto operator()(hir::type::Integer const sub, hir::type::Integer const super) const -> bool
        {
            return sub == super;
        }

        auto operator()(hir::type::Parameterized const& sub, hir::type::Parameterized const& super)
            const -> bool
        {
            return sub.tag.get() == super.tag.get();
        }

        auto operator()(hir::type::Tuple const& sub, hir::type::Tuple const& super) const -> bool
        {
            return sub.types.size() == super.types.size()
               and std::ranges::all_of(std::views::zip(sub.types, super.types), unify());
        }

        auto operator()(hir::type::Array const& sub, hir::type::Array const& super) const -> bool
        {
            return unify(sub.element_type, super.element_type)
               and unify(
                       hir::expression_type(context.hir.expressions[sub.length]),
                       hir::expression_type(context.hir.expressions[super.length]));
        }

        auto operator()(hir::type::Slice const& sub, hir::type::Slice const& super) const -> bool
        {
            return unify(sub.element_type, super.element_type);
        }

        auto operator()(hir::type::Reference const& sub, hir::type::Reference const& super) const
            -> bool
        {
            return unify(sub.referenced_type, super.referenced_type)
               and unify(sub.mutability, super.mutability);
        }

        auto operator()(hir::type::Pointer const& sub, hir::type::Pointer const& super) const
            -> bool
        {
            return unify(sub.pointee_type, super.pointee_type)
               and unify(sub.mutability, super.mutability);
        }

        auto operator()(hir::type::Function const& sub, hir::type::Function const& super) const
            -> bool
        {
            return sub.parameter_types.size() == super.parameter_types.size()
               and unify(sub.return_type, super.return_type)
               and std::ranges::all_of(
                       std::views::zip(sub.parameter_types, super.parameter_types), unify());
        }

        auto operator()(
            hir::type::Enumeration const& sub, hir::type::Enumeration const& super) const -> bool
        {
            // TODO: visit template arguments
            return sub.id == super.id;
        }

        auto operator()(hir::Error, hir::Error) const -> bool
        {
            return true;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(hir::Error, T const&) const -> bool
        {
            return true;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(T const&, hir::Error) const -> bool
        {
            return true;
        }

        auto operator()(auto const&, auto const&) const -> bool
        {
            return false;
        }
    };

    auto unify(
        Context&                          context,
        Inference_state&                  state,
        Type_unification_arguments const& arguments,
        hir::Type_variant const&          sub,
        hir::Type_variant const&          super) -> bool
    {
        Type_unification_visitor const visitor { context, state, arguments, sub, super };
        return visitor.unify(sub, super);
    }
} // namespace

void libresolve::require_subtype_relationship(
    Context&                 context,
    Inference_state&         state,
    hir::Type_variant const& sub,
    hir::Type_variant const& super)
{
    Type_unification_arguments const arguments {
        .goal = Unification_goal::subtype,
    };
    if (not unify(context, state, arguments, sub, super)) {
        cpputil::todo();
    }
}
