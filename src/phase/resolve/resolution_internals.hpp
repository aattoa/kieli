#pragma once

#include "utl/utilities.hpp"
#include "utl/safe_integer.hpp"
#include "phase/lex/lex.hpp"
#include "representation/hir/hir.hpp"
#include "representation/mir/mir.hpp"


namespace resolution {

    struct Nameless_entities {
        std::vector<utl::Wrapper<Implementation_info>>          implementations;
        std::vector<utl::Wrapper<Implementation_template_info>> implementation_templates;
        std::vector<utl::Wrapper<Instantiation_info>>           instantiations;
        std::vector<utl::Wrapper<Instantiation_template_info>>  instantiation_templates;
    };


    namespace constraint {
        struct Explanation {
            utl::Source_view  source_view;
            std::string_view explanatory_note;
        };

        struct Type_equality {
            mir::Type                  constrainer_type;
            mir::Type                  constrained_type;
            tl::optional<Explanation> constrainer_note;
            Explanation                constrained_note;
            bool                       is_deferred = false;
        };
        struct Mutability_equality {
            mir::Mutability constrainer_mutability;
            mir::Mutability constrained_mutability;
            Explanation     constrainer_note;
            Explanation     constrained_note;
            bool            is_deferred = false;
        };
        struct Instance {
            mir::Type                   type;
            utl::Wrapper<Typeclass_info> typeclass;
            Explanation                 explanation;
        };
        struct Struct_field {
            mir::Type            struct_type;
            mir::Type            field_type;
            compiler::Identifier field_identifier;
            Explanation          explanation;
        };
        struct Tuple_field {
            mir::Type   tuple_type;
            mir::Type   field_type;
            utl::Usize   field_index;
            Explanation explanation;
        };
    };


    // Prevents unresolvable circular dependencies
    class Definition_state_guard {
        Definition_state& definition_state;
        int               initial_exception_count;
    public:
        Definition_state_guard(Context&, Definition_state&, ast::Name);
        ~Definition_state_guard();
    };

    // Sets and resets the Self type within classes and impl/inst blocks
    class Self_type_guard {
        tl::optional<mir::Type>& current_self_type;
        tl::optional<mir::Type>  previous_self_type;
    public:
        Self_type_guard(Context&, mir::Type new_self_type);
        ~Self_type_guard();
    };


    using Unsolved_unification_type_variables = std::vector<utl::Wrapper<mir::Type::Variant>>;

    struct [[nodiscard]] Deferred_equality_constraints {
        std::vector<constraint::Type_equality>       types;
        std::vector<constraint::Mutability_equality> mutabilities;
    };

    // Passed to `Context::unify_types`
    struct [[nodiscard]] Type_unification_arguments {
        using Report_unification_failure_callback =
            void (Context&, constraint::Type_equality original, mir::Type left, mir::Type right);
        using Report_recursion_error_callback =
            void (Context&, constraint::Type_equality original, mir::Type variable, mir::Type solution);

        constraint::Type_equality            constraint_to_be_tested;
        Deferred_equality_constraints&       deferred_equality_constraints;
        bool                                 allow_coercion             = false;
        bool                                 do_destructive_unification = false;
        bool                                 gather_variable_solutions  = true;
        Report_unification_failure_callback* report_unification_failure = nullptr;
        Report_recursion_error_callback*     report_recursive_type      = nullptr;
    };
    // Passed to `Context::unify_mutabilities`
    struct [[nodiscard]] Mutability_unification_arguments {
        using Report_unification_failure_callback =
            void (Context&, constraint::Mutability_equality);

        constraint::Mutability_equality      constraint_to_be_tested;
        Deferred_equality_constraints&       deferred_equality_constraints;
        bool                                 allow_coercion             = false;
        bool                                 do_destructive_unification = false;
        bool                                 gather_variable_solutions = true;
        Report_unification_failure_callback* report_unification_failure = nullptr;
    };

    struct [[nodiscard]] Unification_variable_solutions {
        using Types        = utl::Flatmap<mir::Unification_variable_tag, utl::Wrapper<mir::Type      ::Variant>>;
        using Mutabilities = utl::Flatmap<mir::Unification_variable_tag, utl::Wrapper<mir::Mutability::Variant>>;
        Types        types;
        Mutabilities mutabilities;
    };


    // `wrap_type(x)` is shorthand for `utl::wrap(mir::Type::Variant { x })`
    auto wrap_type(mir::Type::Variant&&) -> utl::Wrapper<mir::Type::Variant>;


    class Context {
        utl::Safe_usize current_unification_variable_tag;
        utl::Safe_usize current_template_parameter_tag;

        Deferred_equality_constraints       deferred_equality_constraints;
        Unsolved_unification_type_variables unsolved_unification_type_variables;
        Unification_variable_solutions      unification_variable_solutions;

        utl::Wrapper<mir::Mutability::Variant> immutability_value =
            utl::wrap(mir::Mutability::Variant { mir::Mutability::Concrete { .is_mutable = false } });
        utl::Wrapper<mir::Mutability::Variant> mutability_value =
            utl::wrap(mir::Mutability::Variant { mir::Mutability::Concrete { .is_mutable = true } });

        utl::Wrapper<mir::Type::Variant> unit_type_value             = wrap_type(mir::type::Tuple {});
        utl::Wrapper<mir::Type::Variant> i8_type_value               = wrap_type(mir::type::Integer::i8);
        utl::Wrapper<mir::Type::Variant> i16_type_value              = wrap_type(mir::type::Integer::i16);
        utl::Wrapper<mir::Type::Variant> i32_type_value              = wrap_type(mir::type::Integer::i32);
        utl::Wrapper<mir::Type::Variant> i64_type_value              = wrap_type(mir::type::Integer::i64);
        utl::Wrapper<mir::Type::Variant> u8_type_value               = wrap_type(mir::type::Integer::u8);
        utl::Wrapper<mir::Type::Variant> u16_type_value              = wrap_type(mir::type::Integer::u16);
        utl::Wrapper<mir::Type::Variant> u32_type_value              = wrap_type(mir::type::Integer::u32);
        utl::Wrapper<mir::Type::Variant> u64_type_value              = wrap_type(mir::type::Integer::u64);
        utl::Wrapper<mir::Type::Variant> floating_type_value         = wrap_type(mir::type::Floating {});
        utl::Wrapper<mir::Type::Variant> character_type_value        = wrap_type(mir::type::Character {});
        utl::Wrapper<mir::Type::Variant> boolean_type_value          = wrap_type(mir::type::Boolean {});
        utl::Wrapper<mir::Type::Variant> string_type_value           = wrap_type(mir::type::String {});
        utl::Wrapper<mir::Type::Variant> self_placeholder_type_value = wrap_type(mir::type::Self_placeholder {});
    public:
        hir::Node_context      hir_node_context;
        mir::Node_context      mir_node_context;
        mir::Namespace_context namespace_context;

        mir::Module output_module;

        utl::diagnostics::Builder       diagnostics;
        utl::Source                     source;
        utl::Wrapper<Namespace>         global_namespace;
        Nameless_entities              nameless_entities;
        tl::optional<mir::Type>       current_self_type;
        compiler::Program_string_pool& string_pool;

        compiler::Identifier self_variable_identifier = string_pool.identifiers.make("self");

        Context(
            hir::Node_context            &&,
            mir::Node_context            &&,
            mir::Namespace_context       &&,
            utl::diagnostics::Builder     &&,
            utl::Source                   &&,
            compiler::Program_string_pool &
        ) noexcept;

        Context(Context const&) = delete;
        Context(Context&&) = default;


        [[noreturn]]
        auto error(utl::Source_view, utl::diagnostics::Message_arguments) -> void;


        [[nodiscard]] auto unify_types       (Type_unification_arguments)       -> bool;
        [[nodiscard]] auto unify_mutabilities(Mutability_unification_arguments) -> bool;

        auto solve(constraint::Type_equality       const&) -> void;
        auto solve(constraint::Mutability_equality const&) -> void;
        auto solve(constraint::Instance            const&) -> void;
        auto solve(constraint::Struct_field        const&) -> void;
        auto solve(constraint::Tuple_field         const&) -> void;

        // Clears the deferred constraint vectors, and solves their contained constraints.
        auto solve_deferred_constraints() -> void;


        // Returns a scope with local bindings for the template parameters and the MIR representations of the parameters themselves.
        [[nodiscard]] auto resolve_template_parameters(std::span<hir::Template_parameter>, Namespace&) -> utl::Pair<Scope, std::vector<mir::Template_parameter>>;

        // Returns the signature of the function. Resolves the function body only if the return type is not explicitly specified.
        [[nodiscard]] auto resolve_function_signature(Function_info&) -> mir::Function::Signature&;

        [[nodiscard]] auto resolve_function      (utl::Wrapper<Function_info      >) -> mir::Function      &;
        [[nodiscard]] auto resolve_struct        (utl::Wrapper<Struct_info        >) -> mir::Struct        &;
        [[nodiscard]] auto resolve_enum          (utl::Wrapper<Enum_info          >) -> mir::Enum          &;
        [[nodiscard]] auto resolve_alias         (utl::Wrapper<Alias_info         >) -> mir::Alias         &;
        [[nodiscard]] auto resolve_typeclass     (utl::Wrapper<Typeclass_info     >) -> mir::Typeclass     &;
        [[nodiscard]] auto resolve_implementation(utl::Wrapper<Implementation_info>) -> mir::Implementation&;
        [[nodiscard]] auto resolve_instantiation (utl::Wrapper<Instantiation_info >) -> mir::Instantiation &;

        [[nodiscard]] auto resolve_function_template      (utl::Wrapper<Function_template_info      >) -> mir::Function_template      &;
        [[nodiscard]] auto resolve_struct_template        (utl::Wrapper<Struct_template_info        >) -> mir::Struct_template        &;
        [[nodiscard]] auto resolve_enum_template          (utl::Wrapper<Enum_template_info          >) -> mir::Enum_template          &;
        [[nodiscard]] auto resolve_alias_template         (utl::Wrapper<Alias_template_info         >) -> mir::Alias_template         &;
        [[nodiscard]] auto resolve_typeclass_template     (utl::Wrapper<Typeclass_template_info     >) -> mir::Typeclass_template     &;
        [[nodiscard]] auto resolve_implementation_template(utl::Wrapper<Implementation_template_info>) -> mir::Implementation_template&;
        [[nodiscard]] auto resolve_instantiation_template (utl::Wrapper<Instantiation_template_info >) -> mir::Instantiation_template &;

        [[nodiscard]] auto resolve_type      (hir::Type      &, Scope&, Namespace&) -> mir::Type;
        [[nodiscard]] auto resolve_pattern   (hir::Pattern   &, Scope&, Namespace&) -> mir::Pattern;
        [[nodiscard]] auto resolve_expression(hir::Expression&, Scope&, Namespace&) -> mir::Expression;

        [[nodiscard]] auto resolve_mutability(ast::Mutability, Scope&) -> mir::Mutability;

        [[nodiscard]] auto resolve_class_reference(hir::Class_reference&, Scope&, Namespace&) -> mir::Class_reference;

        [[nodiscard]] auto resolve_method(ast::Name method_name, tl::optional<std::span<hir::Template_argument const>>, mir::Type method_for, Scope&, Namespace&) -> utl::Wrapper<Function_info>;

        [[nodiscard]] auto find_lower(hir::Qualified_name&, Scope&, Namespace&) -> Lower_variant;
        [[nodiscard]] auto find_upper(hir::Qualified_name&, Scope&, Namespace&) -> Upper_variant;

        auto add_to_namespace(Namespace&, ast::Name, Lower_variant) -> void;
        auto add_to_namespace(Namespace&, ast::Name, Upper_variant) -> void;

        // Returns the associated namespace of the given type, or returns nullopt if the type does not have one.
        auto associated_namespace_if(mir::Type) -> tl::optional<utl::Wrapper<Namespace>>;
        // Returns the associated namespace of the given type, or emits an error diagnostic if the type does not have one.
        auto associated_namespace(mir::Type) -> utl::Wrapper<Namespace>;

        auto fresh_unification_mutability_variable(utl::Source_view) -> mir::Mutability;
        auto fresh_general_unification_type_variable(utl::Source_view) -> mir::Type;
        auto fresh_integral_unification_type_variable(utl::Source_view) -> mir::Type;
        auto fresh_template_parameter_reference_tag() -> mir::Template_parameter_tag;

        auto instantiate_function_template(utl::Wrapper<Function_template_info>, std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Function_info>;
        auto instantiate_struct_template  (utl::Wrapper<Struct_template_info>,   std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Struct_info>;
        auto instantiate_enum_template    (utl::Wrapper<Enum_template_info>,     std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Enum_info>;
        auto instantiate_alias_template   (utl::Wrapper<Alias_template_info>,    std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Alias_info>;

        auto instantiate_function_template_with_synthetic_arguments(utl::Wrapper<Function_template_info>, utl::Source_view instantiation_view) -> utl::Wrapper<Function_info>;
        auto instantiate_struct_template_with_synthetic_arguments  (utl::Wrapper<Struct_template_info>,   utl::Source_view instantiation_view) -> utl::Wrapper<Struct_info>;
        auto instantiate_enum_template_with_synthetic_arguments    (utl::Wrapper<Enum_template_info>,     utl::Source_view instantiation_view) -> utl::Wrapper<Enum_info>;
        auto instantiate_alias_template_with_synthetic_arguments   (utl::Wrapper<Alias_template_info>,    utl::Source_view instantiation_view) -> utl::Wrapper<Alias_info>;

        auto   mut_constant(utl::Source_view) -> mir::Mutability;
        auto immut_constant(utl::Source_view) -> mir::Mutability;

        auto unit_type            (utl::Source_view) -> mir::Type;
        auto i8_type              (utl::Source_view) -> mir::Type;
        auto i16_type             (utl::Source_view) -> mir::Type;
        auto i32_type             (utl::Source_view) -> mir::Type;
        auto i64_type             (utl::Source_view) -> mir::Type;
        auto u8_type              (utl::Source_view) -> mir::Type;
        auto u16_type             (utl::Source_view) -> mir::Type;
        auto u32_type             (utl::Source_view) -> mir::Type;
        auto u64_type             (utl::Source_view) -> mir::Type;
        auto floating_type        (utl::Source_view) -> mir::Type;
        auto character_type       (utl::Source_view) -> mir::Type;
        auto boolean_type         (utl::Source_view) -> mir::Type;
        auto string_type          (utl::Source_view) -> mir::Type;
        auto size_type            (utl::Source_view) -> mir::Type;
        auto self_placeholder_type(utl::Source_view) -> mir::Type;

        // Returns a type the value of which is meant to be overwritten
        auto temporary_placeholder_type(utl::Source_view) -> mir::Type;

        template <class T>
        auto literal_type(utl::Source_view const view) -> mir::Type {
            if constexpr (std::same_as<T, utl::Isize>)
                return fresh_integral_unification_type_variable(view);
            else if constexpr (std::same_as<T, utl::Float>)
                return floating_type(view);
            else if constexpr (std::same_as<T, utl::Char>)
                return character_type(view);
            else if constexpr (std::same_as<T, bool>)
                return boolean_type(view);
            else if constexpr (std::same_as<T, compiler::String>)
                return string_type(view);
            else
                static_assert(utl::always_false<T>);
        }
    };

}


DECLARE_FORMATTER_FOR(resolution::constraint::Type_equality);
DECLARE_FORMATTER_FOR(resolution::constraint::Mutability_equality);
DECLARE_FORMATTER_FOR(resolution::constraint::Instance);
DECLARE_FORMATTER_FOR(resolution::constraint::Struct_field);
DECLARE_FORMATTER_FOR(resolution::constraint::Tuple_field);