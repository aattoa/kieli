#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;


#if 0
#define UNIFICATION_LOG(...) fmt::println("[UNIFICATION LOG] " __VA_ARGS__)
#else
#define UNIFICATION_LOG(...) static_assert(requires { fmt::println(__VA_ARGS__); })
#endif


namespace {

    struct Wrapper_shallow_equality {
        template <class T> [[nodiscard]]
        auto operator()(utl::Wrapper<T> const l, utl::Wrapper<T> const r) const noexcept -> bool { return l.is(r); }
    };

    struct [[nodiscard]] Unification_variable_solutions {
        using Type_mappings = utl::Flatmap<utl::Wrapper<mir::Unification_type_variable_state>, mir::Type, Wrapper_shallow_equality>;
        using Mutability_mappings = utl::Flatmap<utl::Wrapper<mir::Unification_mutability_variable_state>, mir::Mutability, Wrapper_shallow_equality>;

        Type_mappings       type_mappings;
        Mutability_mappings mutability_mappings;

        auto destructively_apply() -> void {
            for (auto const& [variable_state, solution] : type_mappings)
                variable_state->solve_with(solution);
            for (auto const& [variable_state, solution] : mutability_mappings)
                variable_state->solve_with(solution);
        }
    };


    // Check whether a unification type variable with the given tag occurs in the given type
    auto occurs_check(mir::Unification_variable_tag, mir::Type) -> bool;

    struct Occurs_check_visitor {
        mir::Unification_variable_tag tag;
        mir::Type                     this_type;

        [[nodiscard]]
        auto recurse(mir::Type const type) const -> bool {
            return occurs_check(tag, type);
        }
        [[nodiscard]]
        auto recurse() const {
            return [*this](mir::Type const type) { return recurse(type); };
        }

        auto operator()(mir::type::Unification_variable const& variable) const {
            return tag == variable.state->as_unsolved().tag;
        }
        auto operator()(mir::type::Array const& array) const {
            return recurse(array.element_type) || recurse(array.array_length->type);
        }
        auto operator()(mir::type::Slice const& slice) const {
            return recurse(slice.element_type);
        }
        auto operator()(mir::type::Tuple const& tuple) const {
            return ranges::any_of(tuple.field_types, recurse());
        }
        auto operator()(mir::type::Function const& function) const {
            return ranges::any_of(function.parameter_types, recurse())
                || recurse(function.return_type);
        }
        auto operator()(mir::type::Reference const& reference) const {
            return recurse(reference.referenced_type);
        }
        auto operator()(mir::type::Pointer const& pointer) const {
            return recurse(pointer.pointed_to_type);
        }
        auto operator()(utl::one_of<mir::type::Structure, mir::type::Enumeration> auto const& user_defined) const {
            if (user_defined.is_application) {
                auto const check_template_argument = [this](mir::Template_argument const& argument) {
                    return utl::match(argument.value,
                        [*this](mir::Type       const  type)       { return recurse(type); },
                        [*this](mir::Expression const& expression) { return recurse(expression.type); },
                        []     (mir::Mutability)                   { return false; }
                    );
                };
                auto& info = utl::get(user_defined.info->template_instantiation_info);
                return ranges::any_of(info.template_arguments, check_template_argument);
            }
            return false;
        }
        auto operator()(utl::one_of<mir::type::Template_parameter_reference, mir::type::Self_placeholder, mir::type::Integer> auto const&) const {
            return false;
        }
        template <class T>
        auto operator()(ast::type::Primitive<T> const&) const {
            return false;
        }
    };

    [[nodiscard]]
    auto occurs_check(mir::Unification_variable_tag const tag, mir::Type const type) -> bool {
        return std::visit(Occurs_check_visitor { .tag = tag, .this_type = type }, *type.flattened_value());
    }


    struct Mutability_unification_visitor {
        libresolve::Mutability_unification_arguments         unification_arguments;
        Unification_variable_solutions::Mutability_mappings& solutions;
        libresolve::Context&                                 context;

        auto unification_failure() -> bool {
            if (auto* const callback = unification_arguments.report_unification_failure) {
                callback(context, unification_arguments.constraint_to_be_tested);
                utl::unreachable();
            }
            return false;
        }

        auto solution(
            utl::Wrapper<mir::Unification_mutability_variable_state> const variable_state,
            mir::Mutability                                          const solution) -> bool
        {
            solutions.add_new_or_abort(variable_state, solution);
            return true;
        }

        auto left_mutability() const noexcept -> mir::Mutability {
            return unification_arguments.constraint_to_be_tested.constrainer_mutability;
        }
        auto right_mutability() const noexcept -> mir::Mutability {
            return unification_arguments.constraint_to_be_tested.constrained_mutability;
        }


        auto operator()(mir::Mutability::Concrete const constrainer, mir::Mutability::Concrete const constrained) -> bool {
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
        auto operator()(mir::Mutability::Parameterized const left, mir::Mutability::Parameterized const right) -> bool {
            return left.tag == right.tag || unification_failure();
        }
        auto operator()(mir::Mutability::Variable const left, auto) -> bool {
            return solution(left.state, right_mutability());
        }
        auto operator()(auto, mir::Mutability::Variable const right) -> bool {
            return solution(right.state, left_mutability());
        }
        auto operator()(mir::Mutability::Variable const left, mir::Mutability::Variable const right) -> bool {
            if (left.state.is(right.state))
                return true;

            // Solve with default value of `immut`
            return solution(left.state, context.immut_constant(left_mutability().source_view()))
                && solution(right.state, context.immut_constant(right_mutability().source_view()));
        }
        auto operator()(auto, auto) -> bool {
            return unification_failure();
        }
    };


    struct Type_unification_visitor {
        mir::Type                                        current_left_type;
        mir::Type                                        current_right_type;
        libresolve::constraint::Type_equality     const& original_constraint;
        libresolve::Type_unification_arguments    const& unification_arguments;
        Unification_variable_solutions                 & solutions;
        libresolve::Context                            & context;

        [[nodiscard]]
        auto recurse(mir::Type const constrainer, mir::Type const constrained) -> bool {
            auto visitor_copy = *this;
            visitor_copy.current_left_type = constrainer;
            visitor_copy.current_right_type = constrained;
            return std::visit(visitor_copy, *constrainer.flattened_value(), *constrained.flattened_value());
        }
        [[nodiscard]]
        auto recurse() {
            return utl::Overload {
                [this](auto const constrainer, auto const constrained) -> bool {
                    return recurse(constrainer, constrained);
                },
                [this](auto const pair) -> bool {
                    return recurse(pair.first, pair.second);
                }
            };
        }

        auto unify_mutability(mir::Mutability const constrainer, mir::Mutability const constrained) -> bool {
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
            return std::visit(visitor, *constrainer.flattened_value(), *constrained.flattened_value())
                || unification_failure();
        }

        [[nodiscard]]
        auto unification_failure() -> bool {
            if (auto* const callback = unification_arguments.report_unification_failure) {
                callback(context, original_constraint, current_left_type, current_right_type);
                utl::unreachable();
            }
            return false;
        }

        [[nodiscard]]
        auto recursion_error(mir::Type const variable, mir::Type const solution) -> bool {
            if (auto* const callback = unification_arguments.report_recursive_type) {
                callback(context, original_constraint, variable, solution);
                utl::unreachable();
            }
            return false;
        }

        auto solution(
            utl::Wrapper<mir::Unification_type_variable_state> const variable_state,
            mir::Type                                          const solution) -> bool
        {
            // UNIFICATION_LOG("adding solution: {} -> {}\n", variable_state, solution);

            if (mir::Type const* const existing_solution = solutions.type_mappings.find(variable_state))
                if (!context.pure_equality_compare(*existing_solution, solution))
                    return unification_failure();

            solutions.type_mappings.add_or_assign(variable_state, solution);
            return true;
        }


        template <utl::one_of<mir::type::Floating, mir::type::Character, mir::type::Boolean, mir::type::String> T>
        auto operator()(T, T) -> bool {
            return true;
        }
        auto operator()(mir::type::Integer const left, mir::type::Integer const right) -> bool {
            return left == right || unification_failure();
        }
        auto operator()(mir::type::Template_parameter_reference const left, mir::type::Template_parameter_reference const right) -> bool {
            return left.tag == right.tag || unification_failure();
        }

        auto operator()(mir::type::Unification_variable const left, mir::type::Unification_variable const right) -> bool {
            if (left.state.is(right.state)) return true;

            auto& left_unsolved = left.state->as_unsolved();
            auto& right_unsolved = right.state->as_unsolved();

            if (right_unsolved.kind.get() == mir::Unification_type_variable_kind::integral)
                left_unsolved.kind.get() = mir::Unification_type_variable_kind::integral;
            utl::append_vector(left_unsolved.classes, std::move(right_unsolved.classes));
            return solution(right.state, current_left_type);
        }

        auto operator()(mir::type::Unification_variable const left, auto&) -> bool {
            auto const& unsolved = left.state->as_unsolved();
            utl::always_assert(unsolved.classes.empty());

            if (unsolved.kind.get() == mir::Unification_type_variable_kind::integral)
                if (!std::holds_alternative<mir::type::Integer>(*current_right_type.pure_value()))
                    return unification_failure();

            if (occurs_check(unsolved.tag, current_right_type))
                return recursion_error(current_left_type, current_right_type);
            else
                return solution(left.state, current_right_type);
        }
        auto operator()(auto&, mir::type::Unification_variable const right) -> bool {
            auto const& unsolved = right.state->as_unsolved();
            utl::always_assert(unsolved.classes.empty());

            if (unsolved.kind.get() == mir::Unification_type_variable_kind::integral)
                if (!std::holds_alternative<mir::type::Integer>(*current_left_type.pure_value()))
                    return unification_failure();

            if (occurs_check(unsolved.tag, current_left_type))
                return recursion_error(current_left_type, current_right_type);
            else
                return solution(right.state, current_left_type);
        }

        auto operator()(mir::type::Reference& left, mir::type::Reference& right) -> bool {
            return recurse(left.referenced_type, right.referenced_type)
                && unify_mutability(left.mutability, right.mutability);
        }
        auto operator()(mir::type::Pointer& left, mir::type::Pointer& right) -> bool {
            return recurse(left.pointed_to_type, right.pointed_to_type)
                && unify_mutability(left.mutability, right.mutability);
        }

        auto operator()(mir::type::Tuple& left, mir::type::Tuple& right) -> bool {
            if (left.field_types.size() == right.field_types.size())
                return ranges::all_of(ranges::views::zip(left.field_types, right.field_types), recurse());
            else
                return unification_failure();
        }

        auto operator()(mir::type::Function& left, mir::type::Function& right) -> bool {
            if (left.parameter_types.size() == right.parameter_types.size())
                return ranges::all_of(ranges::views::zip(left.parameter_types, right.parameter_types), recurse())
                    && recurse(left.return_type, right.return_type);
            else
                return unification_failure();
        }

        template <utl::one_of<mir::type::Structure, mir::type::Enumeration> T>
        auto operator()(T& left, T& right) -> bool {
            if (left.info.is(right.info))
                return true; // Same type
            else if (!left.info->template_instantiation_info || !right.info->template_instantiation_info)
                return unification_failure(); // Unrelated types

            auto& a = utl::get(left.info->template_instantiation_info);
            auto& b = utl::get(right.info->template_instantiation_info);

            if (a.template_instantiated_from.is_not(b.template_instantiated_from))
                return unification_failure(); // Instantiations of different templates

            auto const unify_template_arguments = [&](auto const& pair) {
                return std::visit(utl::Overload {
                    [&](mir::Type const left_argument, mir::Type const right_argument) {
                        return recurse(left_argument, right_argument);
                    },
                    [&](mir::Mutability const left_argument, mir::Mutability const right_argument) {
                        return unify_mutability(left_argument, right_argument);
                    },
                    [](auto const&, auto const&) -> bool {
                        utl::todo();
                    }
                }, pair.first.value, pair.second.value);;
            };

            return ranges::all_of(ranges::views::zip(a.template_arguments, b.template_arguments), unify_template_arguments);
        }

        auto operator()(auto const&, auto const&) -> bool {
            return unification_failure();
        }
    };

}



auto libresolve::Context::unify_mutabilities(Mutability_unification_arguments const arguments) -> bool {
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

    if (result && arguments.do_destructive_unification)
        solutions.destructively_apply();
    return result;
}


auto libresolve::Context::unify_types(Type_unification_arguments const arguments) -> bool {
    UNIFICATION_LOG(
        "unifying {} ~ {}\n",
        arguments.constraint_to_be_tested.constrainer_type,
        arguments.constraint_to_be_tested.constrained_type);

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

    if (result && arguments.do_destructive_unification)
        solutions.destructively_apply();
    return result;
}


auto libresolve::Context::pure_equality_compare(mir::Type const left, mir::Type const right) -> bool {
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
