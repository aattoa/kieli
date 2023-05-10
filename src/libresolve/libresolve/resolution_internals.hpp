#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <libdesugar/hir.hpp>
#include <libresolve/mir.hpp>


namespace resolution {

    struct Nameless_entities {
        std::vector<utl::Wrapper<Implementation_info>>          implementations;
        std::vector<utl::Wrapper<Implementation_template_info>> implementation_templates;
        std::vector<utl::Wrapper<Instantiation_info>>           instantiations;
        std::vector<utl::Wrapper<Instantiation_template_info>>  instantiation_templates;
    };


    namespace constraint {
        struct Explanation {
            utl::Source_view source_view;
            std::string_view explanatory_note;
        };

        struct Type_equality {
            mir::Type                  constrainer_type;
            mir::Type                  constrained_type;
            tl::optional<Explanation> constrainer_note;
            Explanation                constrained_note;
        };
        struct Mutability_equality {
            mir::Mutability constrainer_mutability;
            mir::Mutability constrained_mutability;
            Explanation     constrainer_note;
            Explanation     constrained_note;
        };
        struct Instance { // NOLINT
            mir::Type                    type;
            utl::Wrapper<Typeclass_info> typeclass;
            Explanation                  explanation;
        };
        struct Struct_field { // NOLINT
            mir::Type            struct_type;
            mir::Type            field_type;
            compiler::Identifier field_identifier;
            Explanation          explanation;
        };
        struct Tuple_field { //NOLINT
            mir::Type               tuple_type;
            mir::Type               field_type;
            utl::Strong<utl::Usize> field_index;
            Explanation             explanation;
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


    // Passed to `Context::unify_types`
    struct [[nodiscard]] Type_unification_arguments {
        using Report_unification_failure_callback =
            void (Context&, constraint::Type_equality original, mir::Type left, mir::Type right);
        using Report_recursion_error_callback =
            void (Context&, constraint::Type_equality original, mir::Type variable, mir::Type solution);

        constraint::Type_equality            constraint_to_be_tested;
        bool                                 allow_coercion             = false;
        bool                                 do_destructive_unification = false;
        Report_unification_failure_callback* report_unification_failure = nullptr;
        Report_recursion_error_callback*     report_recursive_type      = nullptr;
    };
    // Passed to `Context::unify_mutabilities`
    struct [[nodiscard]] Mutability_unification_arguments {
        using Report_unification_failure_callback =
            void (Context&, constraint::Mutability_equality);

        constraint::Mutability_equality      constraint_to_be_tested;
        bool                                 allow_coercion             = false;
        bool                                 do_destructive_unification = false;
        Report_unification_failure_callback* report_unification_failure = nullptr;
    };


    struct [[nodiscard]] Loop_info {
        tl::optional<mir::Type>                  break_return_type;
        utl::Strong<hir::expression::Loop::Kind> loop_kind;
    };


    struct Resolution_constants {
        utl::Wrapper<mir::Mutability::Variant> immut;
        utl::Wrapper<mir::Mutability::Variant> mut;
        utl::Wrapper<mir::Type::Variant> unit_type;
        utl::Wrapper<mir::Type::Variant> i8_type;
        utl::Wrapper<mir::Type::Variant> i16_type;
        utl::Wrapper<mir::Type::Variant> i32_type;
        utl::Wrapper<mir::Type::Variant> i64_type;
        utl::Wrapper<mir::Type::Variant> u8_type;
        utl::Wrapper<mir::Type::Variant> u16_type;
        utl::Wrapper<mir::Type::Variant> u32_type;
        utl::Wrapper<mir::Type::Variant> u64_type;
        utl::Wrapper<mir::Type::Variant> floating_type;
        utl::Wrapper<mir::Type::Variant> character_type;
        utl::Wrapper<mir::Type::Variant> boolean_type;
        utl::Wrapper<mir::Type::Variant> string_type;
        utl::Wrapper<mir::Type::Variant> self_placeholder_type;

        explicit Resolution_constants(mir::Node_arena&);
    };

    struct Predefinitions {
        utl::Wrapper<Typeclass_info> copy_class;
        utl::Wrapper<Typeclass_info> drop_class;
    };


    class Context {
        utl::Safe_usize current_unification_variable_tag;
        utl::Safe_usize current_template_parameter_tag;
        utl::Safe_usize current_local_variable_tag;
    public:
        compiler::Compilation_info   compilation_info;
        mir::Node_arena              node_arena;
        mir::Namespace_arena         namespace_arena;
        Resolution_constants         constants;
        tl::optional<Predefinitions> predefinitions_value;
        mir::Module                  output_module;
        utl::Wrapper<Namespace>      global_namespace;
        Nameless_entities            nameless_entities;
        tl::optional<mir::Type>      current_self_type;
        tl::optional<Loop_info>      current_loop_info;

        compiler::Identifier self_variable_id = compilation_info.get()->identifier_pool.make("self");

        explicit Context(
            compiler::Compilation_info&& compilation_info,
            mir::Node_arena           && node_arena,
            mir::Namespace_arena      && namespace_arena) noexcept
            : compilation_info { std::move(compilation_info) }
            , node_arena       { std::move(node_arena) }
            , namespace_arena  { std::move(namespace_arena) }
            , constants        { this->node_arena }
            , global_namespace { wrap(Namespace {}) } {}

        Context(Context const&) = delete;
        Context(Context&&) = default;


        template <class Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node>
            requires requires { node_arena.wrap<Node>(std::move(node)); } && (!std::is_reference_v<Node>)
        {
            return node_arena.wrap<Node>(std::move(node));
        }
        template <class Entity>
        auto wrap(Entity&& entity) -> utl::Wrapper<Entity>
            requires requires { namespace_arena.wrap<Entity>(std::move(entity)); } && (!std::is_reference_v<Entity>)
        {
            return namespace_arena.wrap<Entity>(std::move(entity));
        }
        [[nodiscard]]
        auto wrap() noexcept {
            return [this]<class Arg>(Arg&& arg) -> utl::Wrapper<Arg>
                requires requires (Context context) { context.wrap(std::move(arg)); } && (!std::is_reference_v<Arg>)
            {
                return wrap(std::move(arg));
            };
        }

        // `wrap_type(x)` is shorthand for `wrap(mir::Type::Variant { x })`
        auto wrap_type(mir::Type::Variant&& value) -> utl::Wrapper<mir::Type::Variant> {
            return wrap(std::move(value));
        }


        [[noreturn]] auto error(utl::Source_view, utl::diagnostics::Message_arguments) -> void;

        [[nodiscard]] auto diagnostics() -> utl::diagnostics::Builder&;


        [[nodiscard]] auto unify_types(Type_unification_arguments) -> bool;
        [[nodiscard]] auto unify_mutabilities(Mutability_unification_arguments) -> bool;

        [[nodiscard]] auto pure_equality_compare(mir::Type, mir::Type) -> bool;

        auto solve(constraint::Type_equality       const&) -> void;
        auto solve(constraint::Mutability_equality const&) -> void;
        auto solve(constraint::Instance            const&) -> void;
        auto solve(constraint::Struct_field        const&) -> void;
        auto solve(constraint::Tuple_field         const&) -> void;

        [[nodiscard]] auto predefinitions() -> Predefinitions;

        // Returns a scope with local bindings for the template parameters and the MIR representations of the parameters themselves.
        [[nodiscard]] auto resolve_template_parameters(std::span<hir::Template_parameter>, Namespace&) -> utl::Pair<Scope, std::vector<mir::Template_parameter>>;

        // Returns the signature of the function. Resolves the function body only if the return type is not explicitly specified.
        [[nodiscard]] auto resolve_function_signature(Function_info&) -> mir::Function::Signature&;

        // Solve unsolved unification variables with implicit template parameters
        auto generalize_to(mir::Type, std::vector<mir::Template_parameter>&) -> void;

        // Emit an error diagnostic if the given type contains unsolved unification variables
        auto ensure_non_generalizable(mir::Type, std::string_view type_description) -> void;

        [[nodiscard]] auto resolve_function      (utl::Wrapper<Function_info      >) -> mir::Function      &;
        [[nodiscard]] auto resolve_struct        (utl::Wrapper<Struct_info        >) -> mir::Struct        &;
        [[nodiscard]] auto resolve_enum          (utl::Wrapper<Enum_info          >) -> mir::Enum          &;
        [[nodiscard]] auto resolve_alias         (utl::Wrapper<Alias_info         >) -> mir::Alias         &;
        [[nodiscard]] auto resolve_typeclass     (utl::Wrapper<Typeclass_info     >) -> mir::Typeclass     &;
        [[nodiscard]] auto resolve_implementation(utl::Wrapper<Implementation_info>) -> mir::Implementation&;
        [[nodiscard]] auto resolve_instantiation (utl::Wrapper<Instantiation_info >) -> mir::Instantiation &;

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

        auto fresh_unification_type_variable_state(mir::Unification_type_variable_kind) -> utl::Wrapper<mir::Unification_type_variable_state>;

        auto fresh_general_unification_type_variable(utl::Source_view) -> mir::Type;
        auto fresh_integral_unification_type_variable(utl::Source_view) -> mir::Type;

        auto fresh_unification_mutability_variable(utl::Source_view) -> mir::Mutability;
        auto fresh_template_parameter_reference_tag() -> mir::Template_parameter_tag;
        auto fresh_local_variable_tag()               -> mir::Local_variable_tag;

        auto instantiate_function_template(utl::Wrapper<Function_info>,        std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Function_info>;
        auto instantiate_struct_template  (utl::Wrapper<Struct_template_info>, std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Struct_info>;
        auto instantiate_enum_template    (utl::Wrapper<Enum_template_info>,   std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Enum_info>;
        auto instantiate_alias_template   (utl::Wrapper<Alias_template_info>,  std::span<hir::Template_argument const>, utl::Source_view instantiation_view, Scope&, Namespace&) -> utl::Wrapper<Alias_info>;

        auto instantiate_function_template_with_synthetic_arguments(utl::Wrapper<Function_info>,        utl::Source_view instantiation_view) -> utl::Wrapper<Function_info>;
        auto instantiate_struct_template_with_synthetic_arguments  (utl::Wrapper<Struct_template_info>, utl::Source_view instantiation_view) -> utl::Wrapper<Struct_info>;
        auto instantiate_enum_template_with_synthetic_arguments    (utl::Wrapper<Enum_template_info>,   utl::Source_view instantiation_view) -> utl::Wrapper<Enum_info>;
        auto instantiate_alias_template_with_synthetic_arguments   (utl::Wrapper<Alias_template_info>,  utl::Source_view instantiation_view) -> utl::Wrapper<Alias_info>;

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

        // Returns a type the value of which must be overwritten
        auto temporary_placeholder_type(utl::Source_view) -> mir::Type;

        template <class T>
        auto literal_type(utl::Source_view const view) -> mir::Type {
            if constexpr (utl::one_of<T, compiler::Signed_integer, compiler::Unsigned_integer, compiler::Integer_of_unknown_sign>)
                return fresh_integral_unification_type_variable(view);
            else if constexpr (std::same_as<T, compiler::Floating>)
                return floating_type(view);
            else if constexpr (std::same_as<T, compiler::Character>)
                return character_type(view);
            else if constexpr (std::same_as<T, compiler::Boolean>)
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
