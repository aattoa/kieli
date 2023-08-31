#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {

    struct Wrapper_shallow_equality {
        template <class T>
        [[nodiscard]] auto
        operator()(utl::Wrapper<T> const l, utl::Wrapper<T> const r) const noexcept -> bool
        {
            return l.is(r);
        }
    };

    struct [[nodiscard]] Unification_variable_solutions {
        using Type_mappings = utl::Flatmap<
            utl::Wrapper<hir::Unification_type_variable_state>,
            hir::Type,
            Wrapper_shallow_equality>;
        using Mutability_mappings = utl::Flatmap<
            utl::Wrapper<hir::Unification_mutability_variable_state>,
            hir::Mutability,
            Wrapper_shallow_equality>;

        Type_mappings       type_mappings;
        Mutability_mappings mutability_mappings;

        auto destructively_apply() -> void
        {
            for (auto const& [variable_state, solution] : type_mappings) {
                variable_state->solve_with(solution);
            }
            for (auto const& [variable_state, solution] : mutability_mappings) {
                variable_state->solve_with(solution);
            }
        }
    };

    // Check whether a unification type variable with the given tag occurs in the given type
    auto occurs_check(hir::Unification_variable_tag, hir::Type) -> bool;

    struct Occurs_check_visitor {
        hir::Unification_variable_tag tag;
        hir::Type                     this_type;

        [[nodiscard]] auto recurse(hir::Type const type) const -> bool
        {
            return occurs_check(tag, type);
        }

        [[nodiscard]] auto recurse() const
        {
            return [*this](hir::Type const type) { return recurse(type); };
        }

        auto operator()(hir::type::Unification_variable const& variable) const
        {
            return tag == variable.state->as_unsolved().tag;
        }

        auto operator()(hir::type::Array const& array) const
        {
            return recurse(array.element_type) || recurse(array.array_length->type);
        }

        auto operator()(hir::type::Slice const& slice) const
        {
            return recurse(slice.element_type);
        }

        auto operator()(hir::type::Tuple const& tuple) const
        {
            return ranges::any_of(tuple.field_types, recurse());
        }

        auto operator()(hir::type::Function const& function) const
        {
            return ranges::any_of(function.parameter_types, recurse())
                || recurse(function.return_type);
        }

        auto operator()(hir::type::Reference const& reference) const
        {
            return recurse(reference.referenced_type);
        }

        auto operator()(hir::type::Pointer const& pointer) const
        {
            return recurse(pointer.pointed_to_type);
        }

        auto operator()(utl::one_of<hir::type::Structure, hir::type::Enumeration> auto const&
                            user_defined) const
        {
            if (user_defined.is_application) {
                auto const check_template_argument
                    = [this](hir::Template_argument const& argument) {
                          return utl::match(
                              argument.value,
                              [*this](hir::Type const type) { return recurse(type); },
                              [*this](hir::Expression const& expression) {
                                  return recurse(expression.type);
                              },
                              [](hir::Mutability) { return false; });
                      };
                auto& info = utl::get(user_defined.info->template_instantiation_info);
                return ranges::any_of(info.template_arguments, check_template_argument);
            }
            return false;
        }

        auto operator()(utl::one_of<
                        kieli::built_in_type::Integer,
                        kieli::built_in_type::Floating,
                        kieli::built_in_type::Character,
                        kieli::built_in_type::Boolean,
                        kieli::built_in_type::String,
                        hir::type::Template_parameter_reference,
                        hir::type::Self_placeholder> auto const&) const
        {
            return false;
        }
    };

    [[nodiscard]] auto occurs_check(hir::Unification_variable_tag const tag, hir::Type const type)
        -> bool
    {
        return std::visit(
            Occurs_check_visitor { .tag = tag, .this_type = type }, *type.flattened_value());
    }

    struct Mutability_unification_visitor {
        libresolve::Mutability_unification_arguments         unification_arguments;
        Unification_variable_solutions::Mutability_mappings& solutions;
        libresolve::Context&                                 context;

        auto unification_failure() -> bool
        {
            if (auto* const callback = unification_arguments.report_unification_failure) {
                callback(context, unification_arguments.constraint_to_be_tested);
                utl::unreachable();
            }
            return false;
        }

        auto solution(
            utl::Wrapper<hir::Unification_mutability_variable_state> const variable_state,
            hir::Mutability const                                          solution) -> bool
        {
            solutions.add_new_or_abort(variable_state, solution);
            return true;
        }

        auto left_mutability() const noexcept -> hir::Mutability
        {
            return unification_arguments.constraint_to_be_tested.constrainer_mutability;
        }

        auto right_mutability() const noexcept -> hir::Mutability
        {
            return unification_arguments.constraint_to_be_tested.constrained_mutability;
        }

        auto operator()(
            hir::Mutability::Concrete const constrainer,
            hir::Mutability::Concrete const constrained) -> bool
        {
            if (constrainer.is_mutable.get() == constrained.is_mutable.get()) {
                return true;
            }
            else if (constrainer.is_mutable.get()) {
                // `immut` can not be coerced to `mut`.
                return unification_failure();
            }
            else {
                // `constrainer` is `immut` and `constrained` is `mut`,
                // so unification can occur if coercion is allowed.
                return unification_arguments.allow_coercion;
            }
        }

        auto operator()(
            hir::Mutability::Parameterized const left, hir::Mutability::Parameterized const right)
            -> bool
        {
            return left.tag == right.tag || unification_failure();
        }

        auto operator()(hir::Mutability::Variable const left, auto) -> bool
        {
            return solution(left.state, right_mutability());
        }

        auto operator()(auto, hir::Mutability::Variable const right) -> bool
        {
            return solution(right.state, left_mutability());
        }

        auto operator()(hir::Mutability::Variable const left, hir::Mutability::Variable const right)
            -> bool
        {
            if (left.state.is(right.state)) {
                return true;
            }

            // Solve with default value of `immut`
            return solution(left.state, context.immut_constant(left_mutability().source_view()))
                && solution(right.state, context.immut_constant(right_mutability().source_view()));
        }

        auto operator()(auto, auto) -> bool
        {
            return unification_failure();
        }
    };

    struct Type_unification_visitor {
        hir::Type                                     current_left_type;
        hir::Type                                     current_right_type;
        libresolve::constraint::Type_equality const&  original_constraint;
        libresolve::Type_unification_arguments const& unification_arguments;
        Unification_variable_solutions&               solutions;
        libresolve::Context&                          context;

        [[nodiscard]] auto recurse(hir::Type const constrainer, hir::Type const constrained) -> bool
        {
            auto visitor_copy               = *this;
            visitor_copy.current_left_type  = constrainer;
            visitor_copy.current_right_type = constrained;
            return std::visit(
                visitor_copy, *constrainer.flattened_value(), *constrained.flattened_value());
        }

        [[nodiscard]] auto recurse()
        {
            return utl::Overload { [this](auto const constrainer, auto const constrained) -> bool {
                                      return recurse(constrainer, constrained);
                                  },
                                   [this](auto const pair) -> bool {
                                       return recurse(pair.first, pair.second);
                                   } };
        }

        auto unify_mutability(hir::Mutability const constrainer, hir::Mutability const constrained)
            -> bool
        {
            Mutability_unification_visitor visitor {
                .unification_arguments {
                    .constraint_to_be_tested {
                        .constrainer_mutability = constrainer,
                        .constrained_mutability = constrained,
                        .constrainer_note { .source_view = constrainer.source_view() },
                        .constrained_note { .source_view = constrained.source_view() },
                    },
                    .allow_coercion             = unification_arguments.allow_coercion,
                    .do_destructive_unification = unification_arguments.do_destructive_unification,
                    .report_unification_failure = nullptr, // Handled below
                },
                .solutions = solutions.mutability_mappings,
                .context   = context,
            };
            return std::visit(
                       visitor, *constrainer.flattened_value(), *constrained.flattened_value())
                || unification_failure();
        }

        [[nodiscard]] auto unification_failure() -> bool
        {
            if (auto* const callback = unification_arguments.report_unification_failure) {
                callback(context, original_constraint, current_left_type, current_right_type);
                utl::unreachable();
            }
            return false;
        }

        [[nodiscard]] auto recursion_error(hir::Type const variable, hir::Type const solution)
            -> bool
        {
            if (auto* const callback = unification_arguments.report_recursive_type) {
                callback(context, original_constraint, variable, solution);
                utl::unreachable();
            }
            return false;
        }

        auto solution(
            utl::Wrapper<hir::Unification_type_variable_state> const variable_state,
            hir::Type const                                          solution) -> bool
        {
            // UNIFICATION_LOG("adding solution: {} -> {}\n", variable_state, solution);

            if (hir::Type const* const existing_solution
                = solutions.type_mappings.find(variable_state))
            {
                if (!context.pure_equality_compare(*existing_solution, solution)) {
                    return unification_failure();
                }
            }

            solutions.type_mappings.add_or_assign(variable_state, solution);
            return true;
        }

        template <utl::one_of<
            kieli::built_in_type::Floating,
            kieli::built_in_type::Character,
            kieli::built_in_type::Boolean,
            kieli::built_in_type::String> T>
        auto operator()(T, T) -> bool
        {
            return true;
        }

        auto operator()(
            kieli::built_in_type::Integer const left, kieli::built_in_type::Integer const right)
            -> bool
        {
            return left == right || unification_failure();
        }

        auto operator()(
            hir::type::Template_parameter_reference const left,
            hir::type::Template_parameter_reference const right) -> bool
        {
            return left.tag == right.tag || unification_failure();
        }

        auto operator()(
            hir::type::Unification_variable const left, hir::type::Unification_variable const right)
            -> bool
        {
            if (left.state.is(right.state)) {
                return true;
            }

            auto& left_unsolved  = left.state->as_unsolved();
            auto& right_unsolved = right.state->as_unsolved();

            if (right_unsolved.kind.get() == hir::Unification_type_variable_kind::integral) {
                left_unsolved.kind.get() = hir::Unification_type_variable_kind::integral;
            }
            utl::append_vector(left_unsolved.classes, std::move(right_unsolved.classes));
            return solution(right.state, current_left_type);
        }

        auto operator()(hir::type::Unification_variable const left, auto&) -> bool
        {
            auto const& unsolved = left.state->as_unsolved();
            utl::always_assert(unsolved.classes.empty());

            if (unsolved.kind.get() == hir::Unification_type_variable_kind::integral) {
                if (!std::holds_alternative<kieli::built_in_type::Integer>(
                        *current_right_type.pure_value()))
                {
                    return unification_failure();
                }
            }

            if (occurs_check(unsolved.tag, current_right_type)) {
                return recursion_error(current_left_type, current_right_type);
            }
            else {
                return solution(left.state, current_right_type);
            }
        }

        auto operator()(auto&, hir::type::Unification_variable const right) -> bool
        {
            auto const& unsolved = right.state->as_unsolved();
            utl::always_assert(unsolved.classes.empty());

            if (unsolved.kind.get() == hir::Unification_type_variable_kind::integral) {
                if (!std::holds_alternative<kieli::built_in_type::Integer>(
                        *current_left_type.pure_value()))
                {
                    return unification_failure();
                }
            }

            if (occurs_check(unsolved.tag, current_left_type)) {
                return recursion_error(current_left_type, current_right_type);
            }
            else {
                return solution(right.state, current_left_type);
            }
        }

        auto operator()(hir::type::Reference& left, hir::type::Reference& right) -> bool
        {
            return recurse(left.referenced_type, right.referenced_type)
                && unify_mutability(left.mutability, right.mutability);
        }

        auto operator()(hir::type::Pointer& left, hir::type::Pointer& right) -> bool
        {
            return recurse(left.pointed_to_type, right.pointed_to_type)
                && unify_mutability(left.mutability, right.mutability);
        }

        auto operator()(hir::type::Tuple& left, hir::type::Tuple& right) -> bool
        {
            if (left.field_types.size() == right.field_types.size()) {
                return ranges::all_of(
                    ranges::views::zip(left.field_types, right.field_types), recurse());
            }
            else {
                return unification_failure();
            }
        }

        auto operator()(hir::type::Function& left, hir::type::Function& right) -> bool
        {
            if (left.parameter_types.size() == right.parameter_types.size()) {
                return ranges::all_of(
                           ranges::views::zip(left.parameter_types, right.parameter_types),
                           recurse())
                    && recurse(left.return_type, right.return_type);
            }
            else {
                return unification_failure();
            }
        }

        template <utl::one_of<hir::type::Structure, hir::type::Enumeration> T>
        auto operator()(T& left, T& right) -> bool
        {
            if (left.info.is(right.info)) {
                return true; // Same type
            }
            else if (
                !left.info->template_instantiation_info || !right.info->template_instantiation_info)
            {
                return unification_failure(); // Unrelated types
            }

            auto& a = utl::get(left.info->template_instantiation_info);
            auto& b = utl::get(right.info->template_instantiation_info);

            if (a.template_instantiated_from.is_not(b.template_instantiated_from)) {
                return unification_failure(); // Instantiations of different templates
            }

            auto const unify_template_arguments = [&](auto const& pair) {
                return std::visit(
                    utl::Overload {
                        [&](hir::Type const left_argument, hir::Type const right_argument) {
                            return recurse(left_argument, right_argument);
                        },
                        [&](hir::Mutability const left_argument,
                            hir::Mutability const right_argument) {
                            return unify_mutability(left_argument, right_argument);
                        },
                        [](auto const&, auto const&) -> bool { utl::todo(); } },
                    pair.first.value,
                    pair.second.value);
                ;
            };

            return ranges::all_of(
                ranges::views::zip(a.template_arguments, b.template_arguments),
                unify_template_arguments);
        }

        auto operator()(auto const&, auto const&) -> bool
        {
            return unification_failure();
        }
    };

} // namespace

auto libresolve::Context::unify_mutabilities(Mutability_unification_arguments const arguments)
    -> bool
{
    Unification_variable_solutions solutions;

    Mutability_unification_visitor visitor {
        .unification_arguments = arguments,
        .solutions             = solutions.mutability_mappings,
        .context               = *this,
    };
    bool const result = std::visit(
        visitor,
        *arguments.constraint_to_be_tested.constrainer_mutability.flattened_value(),
        *arguments.constraint_to_be_tested.constrained_mutability.flattened_value());

    if (result && arguments.do_destructive_unification) {
        solutions.destructively_apply();
    }
    return result;
}

auto libresolve::Context::unify_types(Type_unification_arguments const arguments) -> bool
{
    Unification_variable_solutions solutions;

    Type_unification_visitor visitor {
        .current_left_type     = arguments.constraint_to_be_tested.constrainer_type,
        .current_right_type    = arguments.constraint_to_be_tested.constrained_type,
        .original_constraint   = arguments.constraint_to_be_tested,
        .unification_arguments = arguments,
        .solutions             = solutions,
        .context               = *this,
    };
    bool const result = std::visit(
        visitor,
        *arguments.constraint_to_be_tested.constrainer_type.flattened_value(),
        *arguments.constraint_to_be_tested.constrained_type.flattened_value());

    if (result && arguments.do_destructive_unification) {
        solutions.destructively_apply();
    }
    return result;
}

auto libresolve::Context::pure_equality_compare(hir::Type const left, hir::Type const right) -> bool
{
    return unify_types({
        .constraint_to_be_tested {
            .constrainer_type = left,
            .constrained_type = right,
            .constrainer_note = constraint::Explanation { left.source_view() },
            .constrained_note = constraint::Explanation { right.source_view() },
        },
        .allow_coercion             = false,
        .do_destructive_unification = false,
        .report_unification_failure = nullptr,
        .report_recursive_type      = nullptr,
    });
}
