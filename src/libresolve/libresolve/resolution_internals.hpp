#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <libdesugar/ast.hpp>
#include <libresolve/hir.hpp>

namespace libresolve {

    class [[nodiscard]] Scope {
    public:
        struct Variable_binding {
            hir::Type               type;
            hir::Mutability         mutability;
            hir::Local_variable_tag variable_tag;
            bool                    has_been_mentioned = false;
            utl::Source_view        source_view;
        };

        struct Type_binding {
            hir::Type        type;
            bool             has_been_mentioned = false;
            utl::Source_view source_view;
        };

        struct Mutability_binding {
            hir::Mutability  mutability;
            bool             has_been_mentioned = false;
            utl::Source_view source_view;
        };
    private:
        utl::Flatmap<utl::Pooled_string, Variable_binding>   variable_bindings;
        utl::Flatmap<utl::Pooled_string, Type_binding>       type_bindings;
        utl::Flatmap<utl::Pooled_string, Mutability_binding> mutability_bindings;
        Scope*                                               parent = nullptr;
    public:
        auto bind_variable(Context&, utl::Pooled_string, Variable_binding&&) -> void;
        auto bind_type(Context&, utl::Pooled_string, Type_binding&&) -> void;
        auto bind_mutability(Context&, utl::Pooled_string, Mutability_binding&&) -> void;

        auto find_variable(utl::Pooled_string) noexcept -> Variable_binding*;
        auto find_type(utl::Pooled_string) noexcept -> Type_binding*;
        auto find_mutability(utl::Pooled_string) noexcept -> Mutability_binding*;

        auto make_child() noexcept -> Scope;

        auto warn_about_unused_bindings(Context&) -> void;
    };

    using Lower_variant
        = std::variant<utl::Wrapper<Namespace>, utl::Wrapper<Function_info>, hir::Enum_constructor>;

    using Upper_variant = std::variant<
        utl::Wrapper<Struct_info>,
        utl::Wrapper<Enum_info>,
        utl::Wrapper<Alias_info>,
        utl::Wrapper<Typeclass_info>,
        utl::Wrapper<Struct_template_info>,
        utl::Wrapper<Enum_template_info>,
        utl::Wrapper<Alias_template_info>,
        utl::Wrapper<Typeclass_template_info>>;

    using Definition_variant = std::variant<
        utl::Wrapper<Function_info>,
        utl::Wrapper<Struct_info>,
        utl::Wrapper<Enum_info>,
        utl::Wrapper<Alias_info>,
        utl::Wrapper<Typeclass_info>,
        utl::Wrapper<Namespace>,
        utl::Wrapper<Implementation_info>,
        utl::Wrapper<Instantiation_info>,
        utl::Wrapper<Struct_template_info>,
        utl::Wrapper<Enum_template_info>,
        utl::Wrapper<Alias_template_info>,
        utl::Wrapper<Typeclass_template_info>,
        utl::Wrapper<Implementation_template_info>,
        utl::Wrapper<Instantiation_template_info>>;

    struct [[nodiscard]] Namespace {
        std::vector<Definition_variant>                 definitions_in_order;
        utl::Flatmap<utl::Pooled_string, Lower_variant> lower_table;
        utl::Flatmap<utl::Pooled_string, Upper_variant> upper_table;
        tl::optional<utl::Wrapper<Namespace>>           parent;
        tl::optional<compiler::Name_lower>              name;
    };

    enum class Definition_state {
        unresolved,
        resolved,
        currently_on_resolution_stack,
    };

    struct Partially_resolved_function {
        hir::Function::Signature resolved_signature;
        Scope                    signature_scope;
        ast::Expression          unresolved_body;
        compiler::Name_lower     name;
    };

    template <class AST_representation>
    struct Definition_info {
        using Variant = std::variant<AST_representation, hir::From_AST<AST_representation>>;

        Variant                            value;
        utl::Wrapper<Namespace>            home_namespace;
        Definition_state                   state = Definition_state::unresolved;
        decltype(AST_representation::name) name;
    };

    template <class Definition>
        requires requires { &Definition::name; }
    struct Definition_info<ast::definition::Template<Definition>> {
        using Variant = std::variant<
            ast::definition::Template<Definition>,
            hir::From_AST<ast::definition::Template<Definition>>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        hir::Type        parameterized_type_of_this; // One of hir::type::{Structure, Enumeration}
        Definition_state state = Definition_state::unresolved;
        decltype(Definition::name) name;
    };

    template <class Info>
    struct Template_instantiation_info {
        utl::Wrapper<Info>                   template_instantiated_from;
        std::vector<hir::Template_parameter> template_parameters;
        std::vector<hir::Template_argument>  template_arguments;
    };

    template <>
    struct Definition_info<ast::definition::Function> {
        using Variant = std::variant<
            ast::definition::Function,   // Fully unresolved function
            Partially_resolved_function, // Signature resolved, body unresolved
            hir::Function>;              // Fully resolved

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        compiler::Name_lower    name;

        tl::optional<Template_instantiation_info<Function_info>> template_instantiation_info;
    };

    template <>
    struct Definition_info<ast::definition::Struct> {
        using Variant = std::variant<ast::definition::Struct, hir::Struct>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        hir::Type               structure_type;
        Definition_state        state = Definition_state::unresolved;
        compiler::Name_upper    name;

        tl::optional<Template_instantiation_info<Struct_template_info>> template_instantiation_info;
    };

    template <>
    struct Definition_info<ast::definition::Enum> {
        using Variant = std::variant<ast::definition::Enum, hir::Enum>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        hir::Type               enumeration_type;
        Definition_state        state = Definition_state::unresolved;
        compiler::Name_upper    name;

        tl::optional<Template_instantiation_info<Enum_template_info>> template_instantiation_info;

        [[nodiscard]] auto constructor_count() const noexcept -> utl::Usize;
    };

    template <>
    struct Definition_info<ast::definition::Implementation> {
        using Variant = std::variant<ast::definition::Implementation, hir::Implementation>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

    template <>
    struct Definition_info<ast::definition::Instantiation> {
        using Variant = std::variant<ast::definition::Instantiation, hir::Instantiation>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

    template <class Definition>
        requires(!requires { &Definition::name; })
    struct Definition_info<ast::definition::Template<Definition>> {
        using Variant = std::variant<
            ast::definition::Template<Definition>,
            hir::From_AST<ast::definition::Template<Definition>>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

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
            hir::Type                 constrainer_type;
            hir::Type                 constrained_type;
            tl::optional<Explanation> constrainer_note;
            Explanation               constrained_note;
        };

        struct Mutability_equality {
            hir::Mutability constrainer_mutability;
            hir::Mutability constrained_mutability;
            Explanation     constrainer_note;
            Explanation     constrained_note;
        };

        struct Instance { // NOLINT
            hir::Type                    type;
            utl::Wrapper<Typeclass_info> typeclass;
            Explanation                  explanation;
        };

        struct Struct_field { // NOLINT
            hir::Type          struct_type;
            hir::Type          field_type;
            utl::Pooled_string field_identifier;
            Explanation        explanation;
        };

        struct Tuple_field { // NOLINT
            hir::Type                 tuple_type;
            hir::Type                 field_type;
            utl::Explicit<utl::Usize> field_index;
            Explanation               explanation;
        };
    }; // namespace constraint

    // Passed to `Context::unify_types`
    struct [[nodiscard]] Type_unification_arguments {
        using Report_unification_failure_callback
            = void(Context&, constraint::Type_equality original, hir::Type left, hir::Type right);
        using Report_recursion_error_callback = void(
            Context&, constraint::Type_equality original, hir::Type variable, hir::Type solution);

        constraint::Type_equality            constraint_to_be_tested;
        bool                                 allow_coercion             = false;
        bool                                 do_destructive_unification = false;
        Report_unification_failure_callback* report_unification_failure = nullptr;
        Report_recursion_error_callback*     report_recursive_type      = nullptr;
    };

    // Passed to `Context::unify_mutabilities`
    struct [[nodiscard]] Mutability_unification_arguments {
        using Report_unification_failure_callback = void(Context&, constraint::Mutability_equality);

        constraint::Mutability_equality      constraint_to_be_tested;
        bool                                 allow_coercion             = false;
        bool                                 do_destructive_unification = false;
        Report_unification_failure_callback* report_unification_failure = nullptr;
    };

    struct Resolution_constants {
        utl::Wrapper<hir::Mutability::Variant> immut;
        utl::Wrapper<hir::Mutability::Variant> mut;
        utl::Wrapper<hir::Type::Variant>       unit_type;
        utl::Wrapper<hir::Type::Variant>       i8_type;
        utl::Wrapper<hir::Type::Variant>       i16_type;
        utl::Wrapper<hir::Type::Variant>       i32_type;
        utl::Wrapper<hir::Type::Variant>       i64_type;
        utl::Wrapper<hir::Type::Variant>       u8_type;
        utl::Wrapper<hir::Type::Variant>       u16_type;
        utl::Wrapper<hir::Type::Variant>       u32_type;
        utl::Wrapper<hir::Type::Variant>       u64_type;
        utl::Wrapper<hir::Type::Variant>       floating_type;
        utl::Wrapper<hir::Type::Variant>       character_type;
        utl::Wrapper<hir::Type::Variant>       boolean_type;
        utl::Wrapper<hir::Type::Variant>       string_type;
        utl::Wrapper<hir::Type::Variant>       self_placeholder_type;

        explicit Resolution_constants(hir::Node_arena&);
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
        hir::Node_arena              node_arena;
        hir::Namespace_arena         namespace_arena;
        Resolution_constants         constants;
        tl::optional<Predefinitions> predefinitions_value;
        utl::Wrapper<Namespace>      global_namespace;
        Nameless_entities            nameless_entities;
        tl::optional<hir::Type>      current_self_type;

        std::vector<utl::Wrapper<Function_info>> output_functions;

        utl::Pooled_string self_variable_id = compilation_info.get()->identifier_pool.make("self");

        explicit Context(
            compiler::Compilation_info&& compilation_info,
            hir::Node_arena&&            node_arena,
            hir::Namespace_arena&&       namespace_arena) noexcept
            : compilation_info { std::move(compilation_info) }
            , node_arena { std::move(node_arena) }
            , namespace_arena { std::move(namespace_arena) }
            , constants { this->node_arena }
            , global_namespace { wrap(Namespace {}) }
        {}

        Context(Context const&) = delete;
        Context(Context&&)      = default;

        template <class Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node>
            requires requires { node_arena.wrap<Node>(std::move(node)); }
                  && (!std::is_reference_v<Node>)
        {
            return node_arena.wrap<Node>(std::move(node));
        }

        template <class Entity>
        auto wrap(Entity&& entity) -> utl::Wrapper<Entity>
            requires requires { namespace_arena.wrap<Entity>(std::move(entity)); }
                  && (!std::is_reference_v<Entity>)
        {
            return namespace_arena.wrap<Entity>(std::move(entity));
        }

        [[nodiscard]] auto wrap() noexcept
        {
            return [this]<class Arg>(Arg && arg) -> utl::Wrapper<Arg>
                       requires requires(Context context) { context.wrap(std::move(arg)); }
                             && (!std::is_reference_v<Arg>)
            {
                return wrap(std::move(arg));
            };
        }

        // `wrap_type(x)` is shorthand for `wrap(hir::Type::Variant { x })`
        auto wrap_type(hir::Type::Variant&& value) -> utl::Wrapper<hir::Type::Variant>
        {
            return wrap(std::move(value));
        }

        [[noreturn]] auto error(utl::Source_view, utl::diagnostics::Message_arguments const&)
            -> void;

        [[nodiscard]] auto diagnostics() -> utl::diagnostics::Builder&;

        [[nodiscard]] auto unify_types(Type_unification_arguments) -> bool;
        [[nodiscard]] auto unify_mutabilities(Mutability_unification_arguments) -> bool;

        [[nodiscard]] auto pure_equality_compare(hir::Type, hir::Type) -> bool;

        auto solve(constraint::Type_equality const&) -> void;
        auto solve(constraint::Mutability_equality const&) -> void;
        auto solve(constraint::Instance const&) -> void;
        auto solve(constraint::Struct_field const&) -> void;
        auto solve(constraint::Tuple_field const&) -> void;

        [[nodiscard]] auto predefinitions() -> Predefinitions;

        // Returns a scope with local bindings for the template parameters and the HIR
        // representations of the parameters themselves.
        [[nodiscard]] auto
        resolve_template_parameters(std::span<ast::Template_parameter>, Namespace&)
            -> utl::Pair<Scope, std::vector<hir::Template_parameter>>;

        // Returns the signature of the function. Resolves the function body only if the return type
        // is not explicitly specified.
        [[nodiscard]] auto resolve_function_signature(Function_info&) -> hir::Function::Signature&;

        // Solve unsolved unification variables with implicit template parameters
        auto generalize_to(hir::Type, std::vector<hir::Template_parameter>&) -> void;

        // Emit an error diagnostic if the given type contains unsolved unification variables
        auto ensure_non_generalizable(hir::Type, std::string_view type_description) -> void;

        [[nodiscard]] auto resolve_function(utl::Wrapper<Function_info>) -> hir::Function&;
        [[nodiscard]] auto resolve_struct(utl::Wrapper<Struct_info>) -> hir::Struct&;
        [[nodiscard]] auto resolve_enum(utl::Wrapper<Enum_info>) -> hir::Enum&;
        [[nodiscard]] auto resolve_alias(utl::Wrapper<Alias_info>) -> hir::Alias&;
        [[nodiscard]] auto resolve_typeclass(utl::Wrapper<Typeclass_info>) -> hir::Typeclass&;
        [[nodiscard]] auto resolve_implementation(utl::Wrapper<Implementation_info>)
            -> hir::Implementation&;
        [[nodiscard]] auto resolve_instantiation(utl::Wrapper<Instantiation_info>)
            -> hir::Instantiation&;

        [[nodiscard]] auto resolve_struct_template(utl::Wrapper<Struct_template_info>)
            -> hir::Struct_template&;
        [[nodiscard]] auto resolve_enum_template(utl::Wrapper<Enum_template_info>)
            -> hir::Enum_template&;
        [[nodiscard]] auto resolve_alias_template(utl::Wrapper<Alias_template_info>)
            -> hir::Alias_template&;
        [[nodiscard]] auto resolve_typeclass_template(utl::Wrapper<Typeclass_template_info>)
            -> hir::Typeclass_template&;
        [[nodiscard]] auto
            resolve_implementation_template(utl::Wrapper<Implementation_template_info>)
                -> hir::Implementation_template&;
        [[nodiscard]] auto resolve_instantiation_template(utl::Wrapper<Instantiation_template_info>)
            -> hir::Instantiation_template&;

        [[nodiscard]] auto resolve_type(ast::Type&, Scope&, Namespace&) -> hir::Type;
        [[nodiscard]] auto resolve_expression(ast::Expression&, Scope&, Namespace&)
            -> hir::Expression;

        [[nodiscard]] auto resolve_pattern(ast::Pattern&, hir::Type, Scope&, Namespace&)
            -> hir::Pattern;

        [[nodiscard]] auto resolve_mutability(ast::Mutability const&, Scope&) -> hir::Mutability;

        [[nodiscard]] auto resolve_class_reference(ast::Class_reference&, Scope&, Namespace&)
            -> hir::Class_reference;

        [[nodiscard]] auto resolve_method(
            compiler::Name_lower method_name,
            tl::optional<std::span<ast::Template_argument const>>,
            hir::Type method_for,
            Scope&,
            Namespace&) -> utl::Wrapper<Function_info>;

        [[nodiscard]] auto find_lower(ast::Qualified_name&, Scope&, Namespace&) -> Lower_variant;
        [[nodiscard]] auto find_upper(ast::Qualified_name&, Scope&, Namespace&) -> Upper_variant;

        auto add_to_namespace(Namespace&, compiler::Name_lower, Lower_variant) -> void;
        auto add_to_namespace(Namespace&, compiler::Name_upper, Upper_variant) -> void;

        // Returns the associated namespace of the given type, or returns nullopt if the type does
        // not have one.
        auto associated_namespace_if(hir::Type) -> tl::optional<utl::Wrapper<Namespace>>;
        // Returns the associated namespace of the given type, or emits an error diagnostic if the
        // type does not have one.
        auto associated_namespace(hir::Type) -> utl::Wrapper<Namespace>;

        auto fresh_unification_type_variable_state(hir::Unification_type_variable_kind)
            -> utl::Wrapper<hir::Unification_type_variable_state>;

        auto fresh_general_unification_type_variable(utl::Source_view) -> hir::Type;
        auto fresh_integral_unification_type_variable(utl::Source_view) -> hir::Type;

        auto fresh_unification_mutability_variable(utl::Source_view) -> hir::Mutability;
        auto fresh_template_parameter_reference_tag() -> hir::Template_parameter_tag;
        auto fresh_local_variable_tag() -> hir::Local_variable_tag;

        auto instantiate_function_template(
            utl::Wrapper<Function_info>,
            std::span<ast::Template_argument const>,
            utl::Source_view instantiation_view,
            Scope&,
            Namespace&) -> utl::Wrapper<Function_info>;
        auto instantiate_struct_template(
            utl::Wrapper<Struct_template_info>,
            std::span<ast::Template_argument const>,
            utl::Source_view instantiation_view,
            Scope&,
            Namespace&) -> utl::Wrapper<Struct_info>;
        auto instantiate_enum_template(
            utl::Wrapper<Enum_template_info>,
            std::span<ast::Template_argument const>,
            utl::Source_view instantiation_view,
            Scope&,
            Namespace&) -> utl::Wrapper<Enum_info>;
        auto instantiate_alias_template(
            utl::Wrapper<Alias_template_info>,
            std::span<ast::Template_argument const>,
            utl::Source_view instantiation_view,
            Scope&,
            Namespace&) -> utl::Wrapper<Alias_info>;

        auto instantiate_function_template_with_synthetic_arguments(
            utl::Wrapper<Function_info>, utl::Source_view instantiation_view)
            -> utl::Wrapper<Function_info>;
        auto instantiate_struct_template_with_synthetic_arguments(
            utl::Wrapper<Struct_template_info>, utl::Source_view instantiation_view)
            -> utl::Wrapper<Struct_info>;
        auto instantiate_enum_template_with_synthetic_arguments(
            utl::Wrapper<Enum_template_info>, utl::Source_view instantiation_view)
            -> utl::Wrapper<Enum_info>;
        auto instantiate_alias_template_with_synthetic_arguments(
            utl::Wrapper<Alias_template_info>, utl::Source_view instantiation_view)
            -> utl::Wrapper<Alias_info>;

        auto mut_constant(utl::Source_view) -> hir::Mutability;
        auto immut_constant(utl::Source_view) -> hir::Mutability;

        auto unit_type(utl::Source_view) -> hir::Type;
        auto i8_type(utl::Source_view) -> hir::Type;
        auto i16_type(utl::Source_view) -> hir::Type;
        auto i32_type(utl::Source_view) -> hir::Type;
        auto i64_type(utl::Source_view) -> hir::Type;
        auto u8_type(utl::Source_view) -> hir::Type;
        auto u16_type(utl::Source_view) -> hir::Type;
        auto u32_type(utl::Source_view) -> hir::Type;
        auto u64_type(utl::Source_view) -> hir::Type;
        auto floating_type(utl::Source_view) -> hir::Type;
        auto character_type(utl::Source_view) -> hir::Type;
        auto boolean_type(utl::Source_view) -> hir::Type;
        auto string_type(utl::Source_view) -> hir::Type;
        auto size_type(utl::Source_view) -> hir::Type;
        auto self_placeholder_type(utl::Source_view) -> hir::Type;

        // Returns a type the value of which must be overwritten
        auto temporary_placeholder_type(utl::Source_view) -> hir::Type;

        template <compiler::literal T>
        auto literal_type(utl::Source_view const view) -> hir::Type
        {
            if constexpr (std::same_as<T, compiler::Integer>) {
                return fresh_integral_unification_type_variable(view);
            }
            else if constexpr (std::same_as<T, compiler::Floating>) {
                return floating_type(view);
            }
            else if constexpr (std::same_as<T, compiler::Character>) {
                return character_type(view);
            }
            else if constexpr (std::same_as<T, compiler::Boolean>) {
                return boolean_type(view);
            }
            else if constexpr (std::same_as<T, compiler::String>) {
                return string_type(view);
            }
            else {
                static_assert(utl::always_false<T>);
            }
        }
    };

} // namespace libresolve
