#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libdesugar/hir.hpp>


/*

    The Mid-level Intermediate Representation (MIR) is the first intermediate
    program representation that is fully typed. It contains abstract
    information concerning generics, type variables, and other details
    relevant to the type-system. It is produced by resolving the HIR.

*/


namespace libresolve {
    struct [[nodiscard]] Namespace;
    class  [[nodiscard]] Scope;

    template <class>
    struct Definition_info;

    using Function_info = Definition_info<hir::definition::Function>;

#define DEFINE_INFO_NAME(name) \
using name##_info          = Definition_info<hir::definition::name>; \
using name##_template_info = Definition_info<hir::definition::name##_template>
    DEFINE_INFO_NAME(Struct);
    DEFINE_INFO_NAME(Enum);
    DEFINE_INFO_NAME(Alias);
    DEFINE_INFO_NAME(Typeclass);
    DEFINE_INFO_NAME(Implementation);
    DEFINE_INFO_NAME(Instantiation);
#undef DEFINE_INFO_NAME
}



namespace mir {

    struct Function;
    struct Struct;
    struct Enum;
    struct Alias;
    struct Typeclass;
    struct Implementation;
    struct Instantiation;

    template <class>
    struct Template;

    namespace dtl {
        template <class> struct From_HIR_impl;
        template <class> struct To_HIR_impl;
    }
    template <class T> using From_HIR = typename dtl::From_HIR_impl<T>::type;
    template <class T> using To_HIR = typename dtl::To_HIR_impl<T>::type;

#define IMPL_TO_FROM_HIR(name) \
template <> struct dtl::From_HIR_impl<hir::definition::name> : std::type_identity<name> {}; \
template <> struct dtl::To_HIR_impl<name> : std::type_identity<hir::definition::name> {}
    IMPL_TO_FROM_HIR(Function);
    IMPL_TO_FROM_HIR(Struct);
    IMPL_TO_FROM_HIR(Enum);
    IMPL_TO_FROM_HIR(Alias);
    IMPL_TO_FROM_HIR(Typeclass);
    IMPL_TO_FROM_HIR(Implementation);
    IMPL_TO_FROM_HIR(Instantiation);
#undef IMPL_TO_FROM_HIR

    template <template <class> class Definition>
    struct dtl::From_HIR_impl<ast::definition::Template<Definition<hir::HIR_configuration>>>
        : std::type_identity<Template<From_HIR<Definition<hir::HIR_configuration>>>> {};
    template <class Definition>
    struct dtl::To_HIR_impl<Template<Definition>>
        : std::type_identity<ast::definition::Template<To_HIR<Definition>>> {};


    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    class  [[nodiscard]] Type;

    struct [[nodiscard]] Function_parameter;
    struct [[nodiscard]] Template_parameter;
    struct [[nodiscard]] Template_argument;


    struct [[nodiscard]] Class_reference {
        utl::Wrapper<libresolve::Typeclass_info> info;
        utl::Source_view                         source_view;
    };

    struct [[nodiscard]] Unification_variable_tag {
        utl::Usize value;
        constexpr explicit Unification_variable_tag(utl::Usize const value) noexcept
            : value { value } {}
        [[nodiscard]] auto operator==(Unification_variable_tag const&) const noexcept -> bool = default;
    };

    struct [[nodiscard]] Template_parameter_tag {
        utl::Usize value;
        constexpr explicit Template_parameter_tag(utl::Usize const value) noexcept
            : value { value } {}
        [[nodiscard]] auto operator==(Template_parameter_tag const&) const noexcept -> bool = default;
    };

    struct [[nodiscard]] Local_variable_tag {
        utl::Usize value;
        constexpr explicit Local_variable_tag(utl::Usize const value) noexcept
                : value { value } {}
        [[nodiscard]] auto operator==(Local_variable_tag const&) const noexcept -> bool = default;
    };


    class [[nodiscard]] Unification_type_variable_state;
    class [[nodiscard]] Unification_mutability_variable_state;

    class [[nodiscard]] Mutability {
    public:
        struct Concrete {
            utl::Strong<bool> is_mutable;
        };
        struct Variable {
            utl::Wrapper<Unification_mutability_variable_state> state;
        };
        struct Parameterized {
            // The identifier serves no purpose other than debuggability
            compiler::Identifier   identifier;
            Template_parameter_tag tag;
        };
        using Variant = std::variant<Concrete, Variable, Parameterized>;
    private:
        utl::Wrapper<Variant> m_value;
        utl::Source_view      m_source_view;
    public:
        explicit Mutability(utl::Wrapper<Variant>, utl::Source_view) noexcept;

        // Get the wrapped value, but flatten solved unification variables first
        auto flattened_value() const -> utl::Wrapper<Variant>;
        // Get the wrapped value without flattening solved unification variables
        auto pure_value() const noexcept -> utl::Wrapper<Variant>;

        auto source_view() const noexcept -> utl::Source_view;
        auto with(utl::Source_view) const noexcept -> Mutability;
    };

    class [[nodiscard]] Type {
    public:
        struct Variant;
    private:
        utl::Wrapper<Variant> m_value;
        utl::Source_view      m_source_view;
    public:
        explicit Type(utl::Wrapper<Variant>, utl::Source_view) noexcept;

        // Get the wrapped value, but flatten solved unification variables first
        auto flattened_value() const -> utl::Wrapper<Variant>;
        // Get the wrapped value without flattening solved unification variables
        auto pure_value() const noexcept -> utl::Wrapper<Variant>;

        auto source_view() const noexcept -> utl::Source_view;
        auto with(utl::Source_view) const noexcept -> Type;
    };


    namespace type {
        using hir::type::Primitive;
        using hir::type::Integer;
        using hir::type::Floating;
        using hir::type::Character;
        using hir::type::Boolean;
        using hir::type::String;
        // Self within a class
        struct Self_placeholder {};
        struct Tuple {
            std::vector<Type> field_types;
        };
        struct Array {
            Type                     element_type;
            utl::Wrapper<Expression> array_length;
        };
        struct Slice {
            Type element_type;
        };
        struct Function {
            std::vector<Type> parameter_types;
            Type              return_type;
        };
        struct Reference {
            Mutability mutability;
            Type       referenced_type;
        };
        struct Pointer {
            Mutability mutability;
            Type       pointed_to_type;
        };
        struct Structure {
            utl::Wrapper<libresolve::Struct_info> info;
            bool                                  is_application = false;
        };
        struct Enumeration {
            utl::Wrapper<libresolve::Enum_info> info;
            bool                                is_application = false;
        };
        struct Unification_variable {
            utl::Wrapper<Unification_type_variable_state> state;
        };
        struct Template_parameter_reference {
            // The identifier serves no purpose other than debuggability
            utl::Strong<tl::optional<compiler::Identifier>> identifier;
            Template_parameter_tag                          tag;
        };
    }


    struct Type::Variant : std::variant<
        type::Tuple,
        type::Integer,
        type::Floating,
        type::Character,
        type::Boolean,
        type::String,
        type::Self_placeholder,
        type::Array,
        type::Slice,
        type::Function,
        type::Reference,
        type::Pointer,
        type::Structure,
        type::Enumeration,
        type::Unification_variable,
        type::Template_parameter_reference>
    {
        using variant::variant;
        using variant::operator=;
    };


    struct Enum_constructor {
        ast::Name          name;
        tl::optional<Type> payload_type;
        tl::optional<Type> function_type;
        Type               enum_type;
    };

    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
        struct Array_literal {
            std::vector<Expression> elements;
        };
        struct Tuple {
            std::vector<Expression> fields;
        };
        struct Loop {
            utl::Wrapper<Expression> body;
        };
        struct Break {
            utl::Wrapper<Expression> result;
        };
        struct Continue {};
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
        };
        struct Let_binding {
            utl::Wrapper<Pattern>    pattern;
            mir::Type                type;
            utl::Wrapper<Expression> initializer;
        };
        struct Conditional {
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> true_branch;
            utl::Wrapper<Expression> false_branch;
        };
        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };
            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };
        struct Local_variable_reference {
            Local_variable_tag   tag;
            compiler::Identifier identifier;
        };
        struct Struct_initializer {
            std::vector<Expression> initializers;
            Type                    struct_type;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            ast::Name                field_name;
        };
        struct Tuple_field_access {
            utl::Wrapper<Expression> base_expression;
            utl::Usize               field_index {};
            utl::Source_view         field_index_source_view;
        };
        struct Function_reference {
            utl::Wrapper<libresolve::Function_info> info;
            bool                                   is_application = false;
        };
        struct Direct_invocation {
            Function_reference      function;
            std::vector<Expression> arguments;
        };
        struct Indirect_invocation {
            std::vector<Expression> arguments;
            utl::Wrapper<Expression> invocable;
        };
        struct Enum_constructor_reference {
            Enum_constructor constructor;
        };
        struct Direct_enum_constructor_invocation {
            Enum_constructor        constructor;
            std::vector<Expression> arguments;
        };
        struct Sizeof {
            Type inspected_type;
        };
        struct Reference {
            Mutability               mutability;
            utl::Wrapper<Expression> referenced_expression;
        };
        struct Dereference {
            utl::Wrapper<Expression> dereferenced_expression;
        };
        struct Addressof {
            utl::Wrapper<Expression> lvalue;
        };
        struct Unsafe_dereference {
            utl::Wrapper<Expression> pointer;
        };
        struct Move {
            utl::Wrapper<Expression> lvalue;
        };
        struct Hole {};
    }


    struct Expression {
        using Variant = std::variant<
            expression::Literal<kieli::Signed_integer>,
            expression::Literal<kieli::Unsigned_integer>,
            expression::Literal<kieli::Integer_of_unknown_sign>,
            expression::Literal<kieli::Floating>,
            expression::Literal<kieli::Character>,
            expression::Literal<kieli::Boolean>,
            expression::Literal<compiler::String>,
            expression::Array_literal,
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Block,
            expression::Let_binding,
            expression::Conditional,
            expression::Match,
            expression::Local_variable_reference,
            expression::Struct_initializer,
            expression::Struct_field_access,
            expression::Tuple_field_access,
            expression::Function_reference,
            expression::Direct_invocation,
            expression::Indirect_invocation,
            expression::Enum_constructor_reference,
            expression::Direct_enum_constructor_invocation,
            expression::Sizeof,
            expression::Reference,
            expression::Dereference,
            expression::Addressof,
            expression::Unsafe_dereference,
            expression::Move,
            expression::Hole>;

        Variant          value;
        Type             type;
        utl::Source_view source_view;
        Mutability       mutability;
        bool             is_addressable = false;
        bool             is_pure        = false;
    };



    template <class Definition>
    struct Template {
        Definition                                                                 definition;
        std::vector<Template_parameter>                                            parameters;
        std::vector<utl::Wrapper<libresolve::Definition_info<To_HIR<Definition>>>> instantiations;
    };

    struct [[nodiscard]] Self_parameter {
        mir::Mutability   mutability;
        utl::Strong<bool> is_reference;
        utl::Source_view  source_view;
    };

    struct Function {
        struct Signature {
            std::vector<mir::Template_parameter> template_parameters; // empty when not a template
            std::vector<mir::Function_parameter> parameters;
            tl::optional<Self_parameter>         self_parameter;
            ast::Name                            name;
            mir::Type                            return_type;
            mir::Type                            function_type;

            [[nodiscard]] auto is_template() const noexcept -> bool;
        };
        Signature                                            signature;
        Expression                                           body;
        std::vector<utl::Wrapper<libresolve::Function_info>> template_instantiations; // empty when not a template
    };

    struct Struct {
        struct Member { // NOLINT
            ast::Name         name;
            Type              type;
            utl::Strong<bool> is_public;
        };
        std::vector<Member>                 members;
        ast::Name                           name;
        utl::Wrapper<libresolve::Namespace> associated_namespace;
    };
    using Struct_template = Template<Struct>;

    struct Enum {
        std::vector<Enum_constructor>       constructors;
        ast::Name                           name;
        utl::Wrapper<libresolve::Namespace> associated_namespace;
    };
    using Enum_template = Template<Enum>;

    struct Alias {
        Type      aliased_type;
        ast::Name name;
    };
    using Alias_template = Template<Alias>;

    struct Typeclass {
        struct Type_signature {
            std::vector<Class_reference> classes;
        };
        struct Type_template_signature {
            Type_signature                       type_signature;
            std::vector<mir::Template_parameter> template_parameters;
        };
        utl::Flatmap<compiler::Identifier, Function::Signature>         function_signatures;
        utl::Flatmap<compiler::Identifier, Type_signature>              type_signatures;
        utl::Flatmap<compiler::Identifier, Type_template_signature>     type_template_signatures;
        ast::Name                                                       name;
    };
    using Typeclass_template = Template<Typeclass>;

    struct Implementation {
        template <utl::instance_of<libresolve::Definition_info> Info>
        using Map = utl::Flatmap<compiler::Identifier, utl::Wrapper<Info>>;

        struct Definitions {
            Map<libresolve::Function_info>        functions;
            Map<libresolve::Struct_info>          structures;
            Map<libresolve::Struct_template_info> structure_templates;
            Map<libresolve::Enum_info>            enumerations;
            Map<libresolve::Enum_template_info>   enumeration_templates;
            Map<libresolve::Alias_info>           aliases;
            Map<libresolve::Alias_template_info>  alias_templates;
        };
        Definitions definitions;
        Type        self_type;
    };
    using Implementation_template = Template<Implementation>;

    struct Instantiation {
        using Definitions = Implementation::Definitions;
        Definitions     definitions;
        Class_reference class_reference;
        Type            self_type;
    };
    using Instantiation_template = Template<Instantiation>;



    namespace pattern {
        struct Wildcard {};
        template <class T>
        struct Literal {
            T value;
        };
        struct Name {
            Local_variable_tag   variable_tag;
            compiler::Identifier identifier;
            Mutability           mutability;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Slice {
            std::vector<Pattern> element_patterns;
        };
        struct Enum_constructor {
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
            ::mir::Enum_constructor             constructor;
        };
        struct As {
            Name                  alias;
            utl::Wrapper<Pattern> aliased_pattern;
        };
        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression            guard;
        };
    }

    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<kieli::Signed_integer>,
            pattern::Literal<kieli::Unsigned_integer>,
            pattern::Literal<kieli::Integer_of_unknown_sign>,
            pattern::Literal<kieli::Floating>,
            pattern::Literal<kieli::Character>,
            pattern::Literal<kieli::Boolean>,
            pattern::Literal<compiler::String>,
            pattern::Wildcard,
            pattern::Name,
            pattern::Tuple,
            pattern::Slice,
            pattern::Enum_constructor,
            pattern::As,
            pattern::Guarded>;

        Variant          value;
        bool             is_exhaustive_by_itself = false;
        utl::Source_view source_view;
    };



    struct Template_argument {
        using Variant = std::variant<Type, Expression, Mutability>;
        Variant                  value;
        tl::optional<ast::Name> name;
    };

    struct Template_default_argument {
        hir::Template_argument             argument;
        std::shared_ptr<libresolve::Scope> scope;
    };

    struct Template_parameter {
        struct Type_parameter {
            std::vector<Class_reference> classes;
        };
        struct Value_parameter {
            Type type;
        };
        struct Mutability_parameter {};

        using Variant = std::variant<Type_parameter, Value_parameter, Mutability_parameter>;

        Variant                                 value;
        utl::Strong<tl::optional<ast::Name>>    name; // nullopt for implicit template parameters
        tl::optional<Template_default_argument> default_argument;
        Template_parameter_tag                  reference_tag;
        utl::Source_view                        source_view;

        [[nodiscard]] auto is_implicit() const noexcept -> bool;
    };

    struct Function_parameter {
        Pattern pattern;
        Type    type;
    };



    enum class Unification_type_variable_kind { general, integral };

    class Unification_type_variable_state {
    public:
        struct Solved {
            Type solution;
        };
        struct Unsolved {
            Unification_variable_tag                    tag;
            utl::Strong<Unification_type_variable_kind> kind;
            std::vector<Class_reference>                classes;
        };
    private:
        std::variant<Solved, Unsolved> m_value;
    public:
        explicit Unification_type_variable_state(Unsolved&&) noexcept;

        auto solve_with(Type solution) -> void;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current())       noexcept -> Unsolved      &;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current()) const noexcept -> Unsolved const&;
        [[nodiscard]] auto as_solved_if()       noexcept -> Solved      *;
        [[nodiscard]] auto as_solved_if() const noexcept -> Solved const*;
    };

    class Unification_mutability_variable_state {
    public:
        struct Solved {
            Mutability solution;
        };
        struct Unsolved {
            Unification_variable_tag tag;
        };
    private:
        std::variant<Solved, Unsolved> m_value;
    public:
        explicit Unification_mutability_variable_state(Unsolved&&) noexcept;

        auto solve_with(Mutability solution) -> void;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current())       noexcept -> Unsolved      &;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current()) const noexcept -> Unsolved const&;
        [[nodiscard]] auto as_solved_if()       noexcept -> Solved      *;
        [[nodiscard]] auto as_solved_if() const noexcept -> Solved const*;
    };


    using Node_arena = utl::Wrapper_arena<
        Expression,
        Pattern,
        Type::Variant,
        Mutability::Variant,
        Unification_type_variable_state,
        Unification_mutability_variable_state>;

    using Namespace_arena = utl::Wrapper_arena<
        libresolve::Function_info,
        libresolve::Struct_info,
        libresolve::Enum_info,
        libresolve::Alias_info,
        libresolve::Typeclass_info,
        libresolve::Namespace,
        libresolve::Implementation_info,
        libresolve::Instantiation_info,
        libresolve::Struct_template_info,
        libresolve::Enum_template_info,
        libresolve::Alias_template_info,
        libresolve::Typeclass_template_info,
        libresolve::Implementation_template_info,
        libresolve::Instantiation_template_info>;

    struct Module {
        std::vector<utl::Wrapper<libresolve::Function_info>> functions;
    };

}


DECLARE_FORMATTER_FOR(mir::Template_argument);
DECLARE_FORMATTER_FOR(mir::Template_parameter);
DECLARE_FORMATTER_FOR(mir::Class_reference);
DECLARE_FORMATTER_FOR(mir::Mutability);

DECLARE_FORMATTER_FOR(mir::Unification_variable_tag);
DECLARE_FORMATTER_FOR(mir::Unification_type_variable_state::Unsolved);
DECLARE_FORMATTER_FOR(mir::Unification_mutability_variable_state::Unsolved);

DECLARE_FORMATTER_FOR(mir::Expression);
DECLARE_FORMATTER_FOR(mir::Pattern);
DECLARE_FORMATTER_FOR(mir::Type);

DECLARE_FORMATTER_FOR(mir::Function);
DECLARE_FORMATTER_FOR(mir::Struct);
DECLARE_FORMATTER_FOR(mir::Enum);
DECLARE_FORMATTER_FOR(mir::Alias);
DECLARE_FORMATTER_FOR(mir::Typeclass);
DECLARE_FORMATTER_FOR(mir::Implementation);
DECLARE_FORMATTER_FOR(mir::Instantiation);


namespace libresolve {

    class [[nodiscard]] Context;


    class [[nodiscard]] Scope {
    public:
        struct Variable_binding {
            mir::Type               type;
            mir::Mutability         mutability;
            mir::Local_variable_tag variable_tag;
            bool                    has_been_mentioned = false;
            utl::Source_view        source_view;
        };
        struct Type_binding {
            mir::Type        type;
            bool             has_been_mentioned = false;
            utl::Source_view source_view;
        };
        struct Mutability_binding {
            mir::Mutability  mutability;
            bool             has_been_mentioned = false;
            utl::Source_view source_view;
        };
    private:
        utl::Flatmap<compiler::Identifier, Variable_binding>   variable_bindings;
        utl::Flatmap<compiler::Identifier, Type_binding>       type_bindings;
        utl::Flatmap<compiler::Identifier, Mutability_binding> mutability_bindings;
        Scope*                                                 parent = nullptr;
    public:
        auto bind_variable  (Context&, compiler::Identifier, Variable_binding  &&) -> void;
        auto bind_type      (Context&, compiler::Identifier, Type_binding      &&) -> void;
        auto bind_mutability(Context&, compiler::Identifier, Mutability_binding&&) -> void;

        auto find_variable  (compiler::Identifier) noexcept -> Variable_binding*;
        auto find_type      (compiler::Identifier) noexcept -> Type_binding*;
        auto find_mutability(compiler::Identifier) noexcept -> Mutability_binding*;

        auto make_child() noexcept -> Scope;

        auto warn_about_unused_bindings(Context&) -> void;
    };


    using Lower_variant = std::variant<
        utl::Wrapper<Namespace>,
        utl::Wrapper<Function_info>,
        mir::Enum_constructor>;

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
        std::vector<Definition_variant>                   definitions_in_order;
        utl::Flatmap<compiler::Identifier, Lower_variant> lower_table;
        utl::Flatmap<compiler::Identifier, Upper_variant> upper_table;
        tl::optional<utl::Wrapper<Namespace>>            parent;
        tl::optional<ast::Name>                          name;
    };


    enum class Definition_state {
        unresolved,
        resolved,
        currently_on_resolution_stack,
    };


    struct Partially_resolved_function {
        mir::Function::Signature resolved_signature;
        Scope                    signature_scope;
        hir::Expression          unresolved_body;
        ast::Name                name;
    };


    template <class HIR_representation>
    struct Definition_info {
        using Variant = std::variant<HIR_representation, mir::From_HIR<HIR_representation>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;
    };

    template <template <class> class Definition>
    requires requires { &Definition<hir::HIR_configuration>::name; }
    struct Definition_info<ast::definition::Template<Definition<hir::HIR_configuration>>> {
        using Variant = std::variant<
            ast::definition::Template<Definition<hir::HIR_configuration>>,
            mir::From_HIR<ast::definition::Template<Definition<hir::HIR_configuration>>>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               parameterized_type_of_this; // One of mir::type::{Structure, Enumeration
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;
    };

    template <class Info>
    struct Template_instantiation_info {
        utl::Wrapper<Info>                   template_instantiated_from;
        std::vector<mir::Template_parameter> template_parameters;
        std::vector<mir::Template_argument>  template_arguments;
    };

    template <>
    struct Definition_info<hir::definition::Function> {
        using Variant = std::variant<
            hir::definition::Function,          // Fully unresolved function
            hir::definition::Function_template, // Fully unresolved function template
            Partially_resolved_function,        // Signature resolved, body unresolved
            mir::Function>;                     // Fully resolved

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Function_info>> template_instantiation_info;
    };

    template <>
    struct Definition_info<hir::definition::Struct> {
        using Variant = std::variant<hir::definition::Struct, mir::Struct>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               structure_type;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Struct_template_info>> template_instantiation_info;
    };

    template <>
    struct Definition_info<hir::definition::Enum> {
        using Variant = std::variant<hir::definition::Enum, mir::Enum>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               enumeration_type;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Enum_template_info>> template_instantiation_info;

        [[nodiscard]] auto constructor_count() const noexcept -> utl::Usize;
    };

    template <>
    struct Definition_info<hir::definition::Implementation> {
        using Variant = std::variant<hir::definition::Implementation, mir::Implementation>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

    template <>
    struct Definition_info<hir::definition::Instantiation> {
        using Variant = std::variant<hir::definition::Instantiation, mir::Instantiation>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

    template <template <class> class Definition>
    requires (!requires { &Definition<hir::HIR_configuration>::name; })
    struct Definition_info<ast::definition::Template<Definition<hir::HIR_configuration>>> {
        using Variant = std::variant<
            ast::definition::Template<Definition<hir::HIR_configuration>>,
            mir::From_HIR<ast::definition::Template<Definition<hir::HIR_configuration>>>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

}
