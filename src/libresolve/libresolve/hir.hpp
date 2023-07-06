#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libdesugar/ast.hpp>

/*

    The High-level Intermediate Representation (HIR) is the first intermediate
    program representation that is fully typed. It contains abstract
    information concerning generics, type variables, and other details
    relevant to the type-system. It is produced by resolving the AST.

*/


namespace libresolve {
    struct [[nodiscard]] Namespace;
    class  [[nodiscard]] Context;
    class  [[nodiscard]] Scope;

    template <class>
    struct Definition_info;

    using Function_info = Definition_info<ast::definition::Function>;

#define DEFINE_INFO_NAME(name) \
using name##_info          = Definition_info<ast::definition::name>; \
using name##_template_info = Definition_info<ast::definition::name##_template>
    DEFINE_INFO_NAME(Struct);
    DEFINE_INFO_NAME(Enum);
    DEFINE_INFO_NAME(Alias);
    DEFINE_INFO_NAME(Typeclass);
    DEFINE_INFO_NAME(Implementation);
    DEFINE_INFO_NAME(Instantiation);
#undef DEFINE_INFO_NAME
}



namespace hir {

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
        template <class> struct From_AST_impl;
        template <class> struct To_AST_impl;
    }
    template <class T> using From_AST = typename dtl::From_AST_impl<T>::type;
    template <class T> using To_AST = typename dtl::To_AST_impl<T>::type;

#define IMPL_TO_FROM_AST(name) \
template <> struct dtl::From_AST_impl<ast::definition::name> : std::type_identity<name> {}; \
template <> struct dtl::To_AST_impl<name> : std::type_identity<ast::definition::name> {}
    IMPL_TO_FROM_AST(Function);
    IMPL_TO_FROM_AST(Struct);
    IMPL_TO_FROM_AST(Enum);
    IMPL_TO_FROM_AST(Alias);
    IMPL_TO_FROM_AST(Typeclass);
    IMPL_TO_FROM_AST(Implementation);
    IMPL_TO_FROM_AST(Instantiation);
#undef IMPL_TO_FROM_AST

    template <class Definition>
    struct dtl::From_AST_impl<ast::definition::Template<Definition>>
        : std::type_identity<Template<From_AST<Definition>>> {};
    template <class Definition>
    struct dtl::To_AST_impl<Template<Definition>>
        : std::type_identity<ast::definition::Template<To_AST<Definition>>> {};


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
            utl::Explicit<bool> is_mutable;
        };
        struct Variable {
            utl::Wrapper<Unification_mutability_variable_state> state;
        };
        struct Parameterized {
            // The identifier serves no purpose other than debuggability
            utl::Pooled_string     identifier;
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
            utl::Explicit<tl::optional<utl::Pooled_string>> identifier;
            Template_parameter_tag                          tag;
        };
    }


    struct Type::Variant : std::variant<
        compiler::built_in_type::Integer,
        compiler::built_in_type::Floating,
        compiler::built_in_type::Character,
        compiler::built_in_type::Boolean,
        compiler::built_in_type::String,
        type::Self_placeholder,
        type::Array,
        type::Tuple,
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
        compiler::Name_lower name;
        tl::optional<Type>   payload_type;
        tl::optional<Type>   function_type;
        Type                 enum_type;
    };

    namespace expression {
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
            hir::Type                type;
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
            Local_variable_tag tag;
            utl::Pooled_string identifier;
        };
        struct Struct_initializer {
            std::vector<Expression> initializers;
            Type                    struct_type;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            compiler::Name_lower     field_name;
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
            compiler::Integer,
            compiler::Floating,
            compiler::Character,
            compiler::Boolean,
            compiler::String,
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
        std::vector<utl::Wrapper<libresolve::Definition_info<To_AST<Definition>>>> instantiations;
    };

    struct [[nodiscard]] Self_parameter {
        hir::Mutability     mutability;
        utl::Explicit<bool> is_reference;
        utl::Source_view    source_view;
    };

    struct Function {
        struct Signature {
            std::vector<hir::Template_parameter> template_parameters; // empty when not a template
            std::vector<hir::Function_parameter> parameters;
            tl::optional<Self_parameter>         self_parameter;
            compiler::Name_lower                 name;
            hir::Type                            return_type;
            hir::Type                            function_type;

            [[nodiscard]] auto is_template() const noexcept -> bool;
        };
        Signature                                            signature;
        Expression                                           body;
        std::vector<utl::Wrapper<libresolve::Function_info>> template_instantiations; // empty when not a template
    };

    struct Struct {
        struct Member { // NOLINT
            compiler::Name_lower name;
            Type                 type;
            utl::Explicit<bool>  is_public;
        };
        std::vector<Member>                 members;
        compiler::Name_upper                name;
        utl::Wrapper<libresolve::Namespace> associated_namespace;
    };
    using Struct_template = Template<Struct>;

    struct Enum {
        std::vector<Enum_constructor>       constructors;
        compiler::Name_upper                name;
        utl::Wrapper<libresolve::Namespace> associated_namespace;
    };
    using Enum_template = Template<Enum>;

    struct Alias {
        compiler::Name_upper name;
        Type                 aliased_type;
    };
    using Alias_template = Template<Alias>;

    struct Typeclass {
        struct Type_signature {
            std::vector<Class_reference> classes;
        };
        struct Type_template_signature {
            Type_signature                       type_signature;
            std::vector<hir::Template_parameter> template_parameters;
        };
        utl::Flatmap<utl::Pooled_string, Function::Signature> function_signatures;
        utl::Flatmap<utl::Pooled_string, Type_signature>      type_signatures;
        compiler::Name_upper                                  name;
    };
    using Typeclass_template = Template<Typeclass>;

    struct Implementation {
        template <utl::instance_of<libresolve::Definition_info> Info>
        using Map = utl::Flatmap<utl::Pooled_string, utl::Wrapper<Info>>;

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
        struct Name {
            Local_variable_tag variable_tag;
            utl::Pooled_string identifier;
            Mutability         mutability;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Slice {
            std::vector<Pattern> element_patterns;
        };
        struct Enum_constructor {
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
            ::hir::Enum_constructor             constructor;
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
            compiler::Integer,
            compiler::Floating,
            compiler::Character,
            compiler::Boolean,
            compiler::String,
            pattern::Wildcard,
            pattern::Name,
            pattern::Tuple,
            pattern::Slice,
            pattern::Enum_constructor,
            pattern::As,
            pattern::Guarded>;

        Variant             value;
        utl::Explicit<bool> is_exhaustive_by_itself;
        utl::Source_view    source_view;
    };



    struct Template_argument {
        using Variant = std::variant<Type, Expression, Mutability>;
        Variant value;
    };

    struct Template_default_argument {
        ast::Template_argument             argument;
        std::shared_ptr<libresolve::Scope> scope; // FIXME: shared ptr used just for copyability
    };

    struct Template_parameter {
        struct Type_parameter {
            std::vector<Class_reference>       classes;
            tl::optional<compiler::Name_upper> name; // nullopt for implicit type parameters
        };
        struct Value_parameter {
            Type                 type;
            compiler::Name_lower name;
        };
        struct Mutability_parameter {
            compiler::Name_lower name;
        };

        using Variant = std::variant<Type_parameter, Value_parameter, Mutability_parameter>;

        Variant                                 value;
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
            Unification_variable_tag                      tag;
            utl::Explicit<Unification_type_variable_kind> kind;
            std::vector<Class_reference>                  classes;
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


    auto format_to(Expression               const&, std::string&) -> void;
    auto format_to(Type                     const&, std::string&) -> void;
    auto format_to(Pattern                  const&, std::string&) -> void;
    auto format_to(Mutability               const&, std::string&) -> void;
    auto format_to(Function                 const&, std::string&) -> void;
    auto format_to(Struct                   const&, std::string&) -> void;
    auto format_to(Enum                     const&, std::string&) -> void;
    auto format_to(Alias                    const&, std::string&) -> void;
    auto format_to(Typeclass                const&, std::string&) -> void;
    auto format_to(Implementation           const&, std::string&) -> void;
    auto format_to(Instantiation            const&, std::string&) -> void;
    auto format_to(Unification_variable_tag const&, std::string&) -> void;
    auto format_to(Template_parameter       const&, std::string&) -> void;
    auto format_to(Template_argument        const&, std::string&) -> void;
    auto format_to(Function_parameter       const&, std::string&) -> void;

    auto to_string(auto const& x) -> std::string
        requires requires (std::string out) { hir::format_to(x, out); }
    {
        std::string output;
        hir::format_to(x, output);
        return output;
    }

}
