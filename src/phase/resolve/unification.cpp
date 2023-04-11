#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


#if 0
#define UNIFICATION_LOG(...) fmt::println("[UNIFICATION LOG] " __VA_ARGS__)
#else
#define UNIFICATION_LOG(...) ((void)0)
#endif


namespace {

    struct [[nodiscard]] Destructive_unification_map {
        template <class T>
        struct Mapping {
            utl::Wrapper<typename T::Variant> variable;
            utl::Wrapper<typename T::Variant> solution;
        };
        using Type_mapping       = Mapping<mir::Type>;
        using Mutability_mapping = Mapping<mir::Mutability>;

        std::vector<Type_mapping>       type_mappings;
        std::vector<Mutability_mapping> mutability_mappings;

        auto apply(resolution::Unsolved_unification_type_variables& unsolved_unification_type_variables) -> void {
            for (auto const [variable, solution] : type_mappings) {
                if (mir::is_unification_variable(*solution)) {
                    unsolved_unification_type_variables.push_back(variable);
                    unsolved_unification_type_variables.push_back(solution);
                }
                else if (mir::is_unification_variable(*variable)) {
                    UNIFICATION_LOG("solving {} with {}\n", *variable, *solution);
                    *variable = *solution;
                }
            }
            for (auto const [variable, solution] : mutability_mappings) {
                assert(mir::is_unification_variable(*variable));
                *variable = *solution;
            }
        }
    };

    struct Unification_state {
        resolution::Deferred_equality_constraints & deferred_equality_constraints;
        resolution::Unification_variable_solutions& unification_variable_solutions;
        Destructive_unification_map               & destructive_unification_map;

        utl::Usize original_deferred_mutability_equality_constraint_count;
        utl::Usize original_deferred_type_equality_constraint_count;
        utl::Usize original_unification_type_variable_solution_count;
        utl::Usize original_unification_mutability_variable_solution_count;
        utl::Usize original_destructive_unification_mutability_mapping_count;
        utl::Usize original_destructive_unification_type_mapping_count;

        Unification_state(
            resolution::Deferred_equality_constraints & deferred_equality_constraints,
            resolution::Unification_variable_solutions& unification_variable_solutions,
            Destructive_unification_map               & destructive_unification_map)
            : deferred_equality_constraints  { deferred_equality_constraints }
            , unification_variable_solutions { unification_variable_solutions }
            , destructive_unification_map    { destructive_unification_map }
        {
            original_deferred_type_equality_constraint_count       = deferred_equality_constraints.types.size();
            original_deferred_mutability_equality_constraint_count = deferred_equality_constraints.mutabilities.size();

            original_unification_type_variable_solution_count       = unification_variable_solutions.types.size();
            original_unification_mutability_variable_solution_count = unification_variable_solutions.mutabilities.size();

            original_destructive_unification_type_mapping_count       = destructive_unification_map.type_mappings.size();
            original_destructive_unification_mutability_mapping_count = destructive_unification_map.mutability_mappings.size();
        }

        auto restore() -> void {
            utl::resize_down_vector(deferred_equality_constraints.types,             original_deferred_type_equality_constraint_count);
            utl::resize_down_vector(deferred_equality_constraints.mutabilities,      original_deferred_mutability_equality_constraint_count);

            utl::resize_down_vector(unification_variable_solutions.types       .container(), original_unification_type_variable_solution_count);
            utl::resize_down_vector(unification_variable_solutions.mutabilities.container(), original_unification_mutability_variable_solution_count);

            utl::resize_down_vector(destructive_unification_map.type_mappings,       original_destructive_unification_type_mapping_count);
            utl::resize_down_vector(destructive_unification_map.mutability_mappings, original_destructive_unification_mutability_mapping_count);
        }
    };


    // Check whether a unification type variable with the given tag occurs in the given type
    auto occurs_check(mir::Unification_variable_tag, utl::Wrapper<mir::Type::Variant>) -> bool;

    struct Occurs_check_visitor {
        mir::Unification_variable_tag tag;

        [[nodiscard]]
        auto recurse(mir::Type const type) const -> bool {
            return occurs_check(tag, type.value);
        }
        [[nodiscard]]
        auto recurse() const {
            return [*this](mir::Type const type) { return recurse(type); };
        }

        auto operator()(utl::one_of<mir::type::General_unification_variable, mir::type::Integral_unification_variable> auto const variable) {
            return tag == variable.tag;
        }
        auto operator()(mir::type::Array const& array) {
            return recurse(array.element_type) || recurse(array.array_length->type);
        }
        auto operator()(mir::type::Slice const& slice) {
            return recurse(slice.element_type);
        }
        auto operator()(mir::type::Tuple const& tuple) {
            return ranges::any_of(tuple.field_types, recurse());
        }
        auto operator()(mir::type::Function const& function) {
            return ranges::any_of(function.parameter_types, recurse())
                || recurse(function.return_type);
        }
        auto operator()(mir::type::Reference const& reference) {
            return recurse(reference.referenced_type);
        }
        auto operator()(mir::type::Pointer const& pointer) {
            return recurse(pointer.pointed_to_type);
        }
        auto operator()(utl::one_of<mir::type::Structure, mir::type::Enumeration> auto const& user_defined) {
            if (user_defined.is_application) {
                auto const check_template_argument = [this](mir::Template_argument const& argument) {
                    return utl::match(
                        argument.value,
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
        auto operator()(utl::one_of<mir::type::Template_parameter_reference, mir::type::Self_placeholder, mir::type::Integer> auto const&) {
            return false;
        }
        auto operator()(utl::instance_of<ast::type::Primitive> auto const&) {
            return false;
        }
    };

    auto occurs_check(mir::Unification_variable_tag const tag, utl::Wrapper<mir::Type::Variant> const type) -> bool {
        return std::visit(Occurs_check_visitor { .tag = tag }, *type);
    }


    struct Mutability_unification_visitor {
        resolution::Mutability_unification_arguments              unification_arguments;
        Destructive_unification_map                             & destructive_unification_map;
        resolution::Unification_variable_solutions::Mutabilities& solutions;
        resolution::Context                                     & context;

        auto unification_error() -> bool {
            if (auto* const callback = unification_arguments.report_unification_failure) {
                callback(context, unification_arguments.constraint_to_be_tested);
                utl::unreachable();
            }
            return false;
        }

        auto solution(
            mir::Unification_variable_tag const variable_tag,
            mir::Mutability               const variable,
            mir::Mutability               const solution) -> bool
        {
            if (unification_arguments.do_destructive_unification)
                destructive_unification_map.mutability_mappings.emplace_back(variable.value, solution.value);
            solutions.add_new_or_abort(variable_tag, solution.value);
            return true;
        }

        auto left_mutability() const noexcept -> mir::Mutability {
            return unification_arguments.constraint_to_be_tested.constrainer_mutability;
        }
        auto right_mutability() const noexcept -> mir::Mutability {
            return unification_arguments.constraint_to_be_tested.constrained_mutability;
        }


        auto operator()(mir::Mutability::Concrete const constrainer, mir::Mutability::Concrete const constrained) -> bool {
            if (constrainer.is_mutable == constrained.is_mutable) {
                return true;
            }
            else if (constrainer.is_mutable) {
                // `immut` can not be coerced to `mut`.
                return unification_error();
            }
            else {
                // `constrainer` is `immut` and `constrained` is `mut`,
                // so unification can occur if coercion is allowed.
                return unification_arguments.allow_coercion;
            }
        }
        auto operator()(mir::Mutability::Parameterized const left, mir::Mutability::Parameterized const right) -> bool {
            return left.tag == right.tag || unification_error();
        }
        auto operator()(mir::Mutability::Variable const left, auto) -> bool {
            return solution(left.tag, left_mutability(), right_mutability());
        }
        auto operator()(auto, mir::Mutability::Variable const right) -> bool {
            return solution(right.tag, right_mutability(), left_mutability());
        }
        auto operator()(mir::Mutability::Variable const left, mir::Mutability::Variable const right) -> bool {
            if (left.tag == right.tag) {
                return true;
            }

            auto const& constraint = unification_arguments.constraint_to_be_tested;

            if (constraint.is_deferred) {
                // Solve with default value of `immut`
                auto l = left_mutability();
                auto r = right_mutability();
                return solution(left.tag, l, context.immut_constant(l.source_view))
                    && solution(right.tag, r, context.immut_constant(r.source_view));
            }
            else {
                auto constraint_copy = constraint;
                constraint_copy.is_deferred = true;
                unification_arguments.deferred_equality_constraints.mutabilities.push_back(constraint_copy);
                return true;
            }
        }
        auto operator()(auto, auto) -> bool {
            return unification_error();
        }
    };


    struct Type_unification_visitor {
        utl::Wrapper<mir::Type::Variant>                 current_left_type;
        utl::Wrapper<mir::Type::Variant>                 current_right_type;
        resolution::constraint::Type_equality     const& original_constraint;
        resolution::Type_unification_arguments    const& unification_arguments;
        resolution::Unsolved_unification_type_variables& unsolved_unification_type_variables;
        Destructive_unification_map                    & destructive_unification_map;
        resolution::Unification_variable_solutions     & solutions;
        resolution::Context                            & context;

        [[nodiscard]]
        auto recurse(utl::Wrapper<mir::Type::Variant> const constrainer, utl::Wrapper<mir::Type::Variant> const constrained) -> bool {
            auto visitor_copy = *this;
            visitor_copy.current_left_type = constrainer;
            visitor_copy.current_right_type = constrained;
            return std::visit(visitor_copy, *constrainer, *constrained);
        }
        [[nodiscard]]
        auto recurse(mir::Type const constrainer, mir::Type const constrained) -> bool {
            return recurse(constrainer.value, constrained.value);
        }
        [[nodiscard]]
        auto recurse() {
            return utl::Overload {
                [this](auto const constrainer, auto const constrained) -> bool {
                    return recurse(constrainer, constrained);
                },
                [this](auto const pair) -> bool {
                    auto const [l, r] = pair;
                    return recurse(l, r);
                }
            };
        }

        auto unify_mutability(mir::Mutability const constrainer, mir::Mutability const constrained) -> bool {
            Mutability_unification_visitor visitor {
                .unification_arguments {
                    .constraint_to_be_tested {
                        .constrainer_mutability = constrainer,
                        .constrained_mutability = constrained,
                        .constrainer_note { .source_view = constrainer.source_view },
                        .constrained_note { .source_view = constrained.source_view }
                    },
                    .deferred_equality_constraints  = unification_arguments.deferred_equality_constraints,
                    .unification_variable_solutions = unification_arguments.unification_variable_solutions.mutabilities,
                    .allow_coercion                 = unification_arguments.allow_coercion,
                    .do_destructive_unification     = unification_arguments.do_destructive_unification,
                    .report_unification_failure     = nullptr // Handled below
                },
                .destructive_unification_map = destructive_unification_map,
                .solutions                   = solutions.mutabilities,
                .context                     = context
            };
            return std::visit(visitor, *constrainer.value, *constrained.value) || unification_error();
        }

        [[nodiscard]]
        auto unification_error() -> bool {
            if (auto* const callback = unification_arguments.report_unification_failure) {
                callback(context, original_constraint, current_left_type, current_right_type);
                utl::unreachable();
            }
            return false;
        }

        [[nodiscard]]
        auto recursion_error(
            utl::Wrapper<mir::Type::Variant> const variable,
            utl::Wrapper<mir::Type::Variant> const solution) -> bool
        {
            if (auto* const callback = unification_arguments.report_recursive_type) {
                callback(context, original_constraint, variable, solution);
                utl::unreachable();
            }
            return false;
        }

        auto solution(
            mir::Unification_variable_tag    const variable_tag,
            utl::Wrapper<mir::Type::Variant> const variable,
            utl::Wrapper<mir::Type::Variant> const solution) -> bool
        {
            UNIFICATION_LOG("adding solution: {} -> {}\n", variable_tag, solution);

            if (unification_arguments.do_destructive_unification)
                destructive_unification_map.type_mappings.emplace_back(variable, solution);

            if (utl::wrapper auto const* const existing_solution = solutions.types.find(variable_tag))
                if (!context.pure_try_equate_types(*existing_solution, solution))
                    return unification_error();

            solutions.types.add_or_assign(variable_tag, solution);
            return true;
        }

        auto defer(resolution::constraint::Type_equality constraint) -> bool {
            constraint.is_deferred = true;
            unification_arguments.deferred_equality_constraints.types.push_back(constraint);
            return true;
        }


        template <utl::one_of<mir::type::Floating, mir::type::Character, mir::type::Boolean, mir::type::String> T>
        auto operator()(T, T) -> bool {
            return true;
        }
        auto operator()(mir::type::Integer const left, mir::type::Integer const right) -> bool {
            return left == right || unification_error();
        }
        auto operator()(mir::type::Template_parameter_reference const left, mir::type::Template_parameter_reference const right) -> bool {
            return left.tag == right.tag || unification_error();
        }

        auto operator()(mir::unification_variable auto& left, mir::unification_variable auto& right) -> bool {
            if (left.tag == right.tag) {
                return true;
            }
            else if (original_constraint.is_deferred) {
                unsolved_unification_type_variables.push_back(current_left_type);
                unsolved_unification_type_variables.push_back(current_right_type);
                return solution(left.tag, current_left_type, current_right_type)
                    && solution(right.tag, current_right_type, current_left_type);
            }
            else {
                return defer(original_constraint);
            }
        }


        auto operator()(mir::type::Integer, mir::type::Integral_unification_variable const right) -> bool {
            return solution(right.tag, current_right_type, current_left_type);
        }
        auto operator()(mir::type::Integral_unification_variable const left, mir::type::Integer) -> bool {
            return solution(left.tag, current_left_type, current_right_type);
        }

        template <class Right> requires (!mir::unification_variable<Right>)
        auto operator()(mir::type::General_unification_variable& left, Right const&) -> bool {
            if (occurs_check(left.tag, current_right_type))
                return recursion_error(current_left_type, current_right_type);
            else
                return solution(left.tag, current_left_type, current_right_type);
        }
        template <class Left> requires (!mir::unification_variable<Left>)
        auto operator()(Left const&, mir::type::General_unification_variable const right) -> bool {
            if (occurs_check(right.tag, current_left_type))
                return recursion_error(current_right_type, current_left_type);
            else
                return solution(right.tag, current_right_type, current_left_type);
        }

        auto operator()(mir::type::Reference& left, mir::type::Reference& right) -> bool {
            return recurse(left.referenced_type, right.referenced_type)
                && unify_mutability(left.mutability, right.mutability);
        }
        auto operator()(mir::type::Pointer& left, mir::type::Pointer& right) -> bool {
            return recurse(left.pointed_to_type, right.pointed_to_type)
                && unify_mutability(left.mutability, right.mutability);
        }

        /*auto operator()(mir::type::Array& left, mir::type::Array& right) -> bool {
            recurse(left.element_type, right.element_type);
        }*/

        auto operator()(mir::type::Tuple& left, mir::type::Tuple& right) -> bool {
            if (left.field_types.size() == right.field_types.size())
                return ranges::all_of(ranges::views::zip(left.field_types, right.field_types), recurse());
            else
                return unification_error();
        }

        auto operator()(mir::type::Function& left, mir::type::Function& right) -> bool {
            if (left.parameter_types.size() == right.parameter_types.size())
                return ranges::all_of(ranges::views::zip(left.parameter_types, right.parameter_types), recurse())
                    && recurse(left.return_type, right.return_type);
            else
                return unification_error();
        }

        template <utl::one_of<mir::type::Structure, mir::type::Enumeration> T>
        auto operator()(T& left, T& right) -> bool {
            if (&*left.info == &*right.info)
                return true; // Same type
            else if (!left.info->template_instantiation_info || !right.info->template_instantiation_info)
                return unification_error(); // Unrelated types

            auto& a = utl::get(left.info->template_instantiation_info);
            auto& b = utl::get(right.info->template_instantiation_info);

            if (&*a.template_instantiated_from != &*b.template_instantiated_from)
                return unification_error(); // Instantiations of different templates

            auto const unify_template_arguments = [&](auto const& pair) {
                auto const& [l, r] = pair;
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
                }, l.value, r.value);;
            };

            return ranges::all_of(ranges::views::zip(a.template_arguments, b.template_arguments), unify_template_arguments);
        }

        auto operator()(auto const&, auto const&) -> bool {
            return unification_error();
        }
    };

}



auto resolution::Context::unify_mutabilities(Mutability_unification_arguments const arguments) -> bool {
    Destructive_unification_map destructive_unification_map;

    Unification_state unification_state {
        arguments.deferred_equality_constraints,
        unification_variable_solutions,
        destructive_unification_map,
    };

    Mutability_unification_visitor visitor {
        .unification_arguments       = arguments,
        .destructive_unification_map = destructive_unification_map,
        .solutions                   = unification_variable_solutions.mutabilities,
        .context                     = *this
    };

    auto const constraint = arguments.constraint_to_be_tested;
    if (std::visit(visitor, *constraint.constrainer_mutability.value, *constraint.constrained_mutability.value)) {
        if (arguments.do_destructive_unification)
            destructive_unification_map.apply(unsolved_unification_type_variables);
        return true;
    }
    else {
        unification_state.restore();
        return false;
    }
}


auto resolution::Context::unify_types(Type_unification_arguments const arguments) -> bool {
    UNIFICATION_LOG(
        "unifying {} ~ {}\n",
        arguments.constraint_to_be_tested.constrainer_type,
        arguments.constraint_to_be_tested.constrained_type);

    Destructive_unification_map destructive_unification_map;

    Unification_state unification_state {
        arguments.deferred_equality_constraints,
        unification_variable_solutions,
        destructive_unification_map,
    };

    constraint::Type_equality const& constraint = arguments.constraint_to_be_tested;

    Type_unification_visitor visitor {
        .current_left_type                   = constraint.constrainer_type.value,
        .current_right_type                  = constraint.constrained_type.value,
        .original_constraint                 = constraint,
        .unification_arguments               = arguments,
        .unsolved_unification_type_variables = unsolved_unification_type_variables,
        .destructive_unification_map         = destructive_unification_map,
        .solutions                           = arguments.unification_variable_solutions,
        .context                             = *this
    };

    if (std::visit(visitor, *constraint.constrainer_type.value, *constraint.constrained_type.value)) {
        if (arguments.do_destructive_unification) {
            destructive_unification_map.apply(unsolved_unification_type_variables);
        }
        return true;
    }
    else {
        unification_state.restore();
        return false;
    }
}

auto resolution::Context::pure_try_equate_types(
    utl::Wrapper<mir::Type::Variant> const left,
    utl::Wrapper<mir::Type::Variant> const right) -> bool
{
    Deferred_equality_constraints temporary_deferred_constraints;
    Unification_variable_solutions temporary_solutions;

    auto const try_unify = [&](utl::Wrapper<mir::Type::Variant> const l, utl::Wrapper<mir::Type::Variant> const r) {
        return unify_types({
            .constraint_to_be_tested {
                .constrainer_type { l,  utl::Source_view::dummy() },
                .constrained_type { r, utl::Source_view::dummy() },
                .constrainer_note = constraint::Explanation { utl::Source_view::dummy() },
                .constrained_note = constraint::Explanation { utl::Source_view::dummy() },
            },
            .deferred_equality_constraints  = temporary_deferred_constraints,
            .unification_variable_solutions = temporary_solutions,
            .allow_coercion                 = false,
            .do_destructive_unification     = false,
            .report_unification_failure     = nullptr,
            .report_recursive_type          = nullptr,
        });
    };

    if (!try_unify(left, right))
        return false;
    utl::always_assert(temporary_deferred_constraints.mutabilities.empty());

    return ranges::all_of(temporary_deferred_constraints.types, [&](auto const& constraint) {
        return try_unify(constraint.constrainer_type.value, constraint.constrained_type.value);
    });
}