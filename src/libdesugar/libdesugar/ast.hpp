#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libphase/phase.hpp>

/*

    The Abstract Syntax Tree (AST) is a high level structured representation
    of a program's syntax, much like the CST, just without the exact source
    information. It is produced by desugaring the CST.

    For example, the following CST node:
        while a { b }

    would be desugared to the following AST node:
        loop { if a { b } else { break () } }

*/

namespace ast {

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Definition;

    struct Wildcard {
        utl::Source_range source_range;
    };

    struct Mutability {
        struct Concrete {
            utl::Explicit<bool> is_mutable;
        };

        struct Parameterized {
            kieli::Name_lower name;
        };

        using Variant = std::variant<Concrete, Parameterized>;

        Variant             value;
        utl::Explicit<bool> is_explicit;
        utl::Source_range   source_range;
    };

    struct Template_argument {
        using Variant = std::variant< //
            utl::Wrapper<Type>,
            utl::Wrapper<Expression>,
            Mutability,
            Wildcard>;
        Variant value;
    };

    struct Qualifier {
        std::optional<std::vector<Template_argument>> template_arguments;
        kieli::Name_dynamic                           name;
        utl::Source_range                             source_range;
    };

    struct Root_qualifier {
        struct Global {};

        std::variant<Global, utl::Wrapper<Type>> value;
    };

    struct Qualified_name {
        std::vector<Qualifier>        middle_qualifiers;
        std::optional<Root_qualifier> root_qualifier;
        kieli::Name_dynamic           primary_name;

        [[nodiscard]] auto is_upper() const noexcept -> bool;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    struct Class_reference {
        std::optional<std::vector<Template_argument>> template_arguments;
        Qualified_name                                name;
        utl::Source_range                             source_range;
    };

    struct Template_type_parameter {
        using Default = std::variant<utl::Wrapper<Type>, Wildcard>;
        kieli::Name_upper            name;
        std::vector<Class_reference> classes;
        std::optional<Default>       default_argument;
    };

    struct Template_value_parameter {
        using Default = std::variant<utl::Wrapper<Expression>, Wildcard>;
        kieli::Name_lower                 name;
        std::optional<utl::Wrapper<Type>> type;
        std::optional<Default>            default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        kieli::Name_lower      name;
        std::optional<Default> default_argument;
    };

    struct Template_parameter {
        using Variant = std::variant<
            Template_type_parameter,
            Template_value_parameter,
            Template_mutability_parameter>;

        Variant           value;
        utl::Source_range source_range;

        static auto kind_description(Variant const&) noexcept -> std::string_view;
    };

    struct Function_argument {
        utl::Wrapper<Expression>         expression;
        std::optional<kieli::Name_lower> argument_name;
    };

    struct Function_parameter {
        utl::Wrapper<Pattern>                   pattern;
        std::optional<utl::Wrapper<Type>>       type;
        std::optional<utl::Wrapper<Expression>> default_argument;
    };

    namespace expression {
        struct Array_literal {
            std::vector<Expression> elements;
        };

        struct Self {};

        struct Variable {
            Qualified_name name;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            enum class Source { plain_loop, while_loop, for_loop };
            utl::Wrapper<Expression> body;
            utl::Explicit<Source>    source;
        };

        struct Continue {};

        struct Break {
            utl::Wrapper<Expression> result;
        };

        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
        };

        struct Invocation {
            std::vector<Function_argument> arguments;
            utl::Wrapper<Expression>       invocable;
        };

        struct Unit_initializer {
            Qualified_name constructor;
        };

        struct Tuple_initializer {
            Qualified_name                        constructor;
            std::vector<utl::Wrapper<Expression>> initializers;
        };

        struct Struct_initializer {
            struct Field {
                kieli::Name_lower        name;
                utl::Wrapper<Expression> expression;
            };

            Qualified_name     constructor;
            std::vector<Field> initializers;
        };

        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left;
            utl::Wrapper<Expression> right;
            kieli::Identifier        op;
        };

        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            kieli::Name_lower        field_name;
        };

        struct Tuple_field_access {
            utl::Wrapper<Expression>   base_expression;
            utl::Explicit<std::size_t> field_index;
            utl::Source_range          field_index_source_range;
        };

        struct Array_index_access {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Expression> index_expression;
        };

        struct Method_invocation {
            std::vector<Function_argument>                function_arguments;
            std::optional<std::vector<Template_argument>> template_arguments;
            utl::Wrapper<Expression>                      base_expression;
            kieli::Name_lower                             method_name;
        };

        struct Conditional {
            enum class Source { normal_conditional, elif_conditional, while_loop_body };
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> true_branch;
            utl::Wrapper<Expression> false_branch;
            utl::Explicit<Source>    source;
            utl::Explicit<bool>      has_explicit_false_branch;
        };

        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };

            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };

        struct Template_application {
            std::vector<Template_argument> template_arguments;
            Qualified_name                 name;
        };

        struct Type_cast {
            utl::Wrapper<Expression> expression;
            utl::Wrapper<Type>       target_type;
        };

        struct Type_ascription {
            utl::Wrapper<Expression> expression;
            utl::Wrapper<Type>       ascribed_type;
        };

        struct Let_binding {
            utl::Wrapper<Pattern>             pattern;
            utl::Wrapper<Expression>          initializer;
            std::optional<utl::Wrapper<Type>> type;
        };

        struct Local_type_alias {
            kieli::Name_upper  alias_name;
            utl::Wrapper<Type> aliased_type;
        };

        struct Ret {
            std::optional<utl::Wrapper<Expression>> returned_expression;
        };

        struct Sizeof {
            utl::Wrapper<Type> inspected_type;
        };

        struct Reference {
            Mutability               mutability;
            utl::Wrapper<Expression> referenced_expression;
        };

        struct Reference_dereference {
            utl::Wrapper<Expression> dereferenced_expression;
        };

        struct Pointer_dereference {
            utl::Wrapper<Expression> pointer_expression;
        };

        struct Addressof {
            utl::Wrapper<Expression> lvalue_expression;
        };

        struct Unsafe {
            utl::Wrapper<Expression> expression;
        };

        struct Move {
            utl::Wrapper<Expression> lvalue;
        };

        struct Meta {
            utl::Wrapper<Expression> expression;
        };

        struct Hole {};
    } // namespace expression

    struct Expression {
        using Variant = std::variant<
            kieli::Integer,
            kieli::Floating,
            kieli::Character,
            kieli::Boolean,
            kieli::String,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Block,
            expression::Invocation,
            expression::Unit_initializer,
            expression::Tuple_initializer,
            expression::Struct_initializer,
            expression::Binary_operator_invocation,
            expression::Struct_field_access,
            expression::Tuple_field_access,
            expression::Array_index_access,
            expression::Method_invocation,
            expression::Conditional,
            expression::Match,
            expression::Template_application,
            expression::Type_cast,
            expression::Type_ascription,
            expression::Let_binding,
            expression::Local_type_alias,
            expression::Ret,
            expression::Sizeof,
            expression::Reference,
            expression::Addressof,
            expression::Reference_dereference,
            expression::Pointer_dereference,
            expression::Unsafe,
            expression::Move,
            expression::Meta,
            expression::Hole>;

        Variant           value;
        utl::Source_range source_range;
    };

    namespace pattern {
        struct Name {
            kieli::Name_lower name;
            Mutability        mutability;
        };

        struct Field {
            kieli::Name_lower                    name;
            std::optional<utl::Wrapper<Pattern>> pattern;
        };

        struct Struct_constructor {
            std::vector<Field> fields;
        };

        struct Tuple_constructor {
            utl::Wrapper<Pattern> pattern;
        };

        struct Unit_constructor {};

        using Constructor_body
            = std::variant<Struct_constructor, Tuple_constructor, Unit_constructor>;

        struct Constructor {
            Qualified_name   name;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            kieli::Name_upper name;
            Constructor_body  body;
        };

        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> element_patterns;
        };

        struct Alias {
            kieli::Name_lower     alias_name;
            Mutability            alias_mutability;
            utl::Wrapper<Pattern> aliased_pattern;
        };

        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression            guard;
        };
    } // namespace pattern

    struct Pattern {
        using Variant = std::variant<
            kieli::Integer,
            kieli::Floating,
            kieli::Character,
            kieli::Boolean,
            kieli::String,
            Wildcard,
            pattern::Name,
            pattern::Constructor,
            pattern::Abbreviated_constructor,
            pattern::Tuple,
            pattern::Slice,
            pattern::Alias,
            pattern::Guarded>;

        Variant           value;
        utl::Source_range source_range;
    };

    namespace type {
        struct Self {};

        struct Typename {
            Qualified_name name;
        };

        struct Tuple {
            std::vector<Type> field_types;
        };

        struct Array {
            utl::Wrapper<Type>       element_type;
            utl::Wrapper<Expression> array_length;
        };

        struct Slice {
            utl::Wrapper<Type> element_type;
        };

        struct Function {
            std::vector<Type>  argument_types;
            utl::Wrapper<Type> return_type;
        };

        struct Typeof {
            utl::Wrapper<Expression> inspected_expression;
        };

        struct Reference {
            utl::Wrapper<Type> referenced_type;
            Mutability         mutability;
        };

        struct Pointer {
            utl::Wrapper<Type> pointed_to_type;
            Mutability         mutability;
        };

        struct Instance_of {
            std::vector<Class_reference> classes;
        };

        struct Template_application {
            std::vector<Template_argument> arguments;
            Qualified_name                 name;
        };
    } // namespace type

    struct Type {
        using Variant = std::variant<
            kieli::built_in_type::Integer,
            kieli::built_in_type::Floating,
            kieli::built_in_type::Character,
            kieli::built_in_type::Boolean,
            kieli::built_in_type::String,
            Wildcard,
            type::Self,
            type::Typename,
            type::Tuple,
            type::Array,
            type::Slice,
            type::Function,
            type::Typeof,
            type::Reference,
            type::Pointer,
            type::Instance_of,
            type::Template_application>;

        Variant           value;
        utl::Source_range source_range;
    };

    struct Self_parameter {
        Mutability          mutability;
        utl::Explicit<bool> is_reference;
        utl::Source_range   source_range;
    };

    struct Function_signature {
        std::vector<Template_parameter> template_parameters;
        std::vector<Function_parameter> function_parameters;
        std::optional<Self_parameter>   self_parameter;
        std::optional<Type>             return_type;
        kieli::Name_lower               name;
    };

    struct Type_signature {
        std::vector<Template_parameter> template_parameters;
        std::vector<Class_reference>    classes;
        kieli::Name_upper               name;
    };

    namespace definition {
        using Template_parameters = std::optional<std::vector<Template_parameter>>;

        struct Function {
            Function_signature  signature;
            Expression          body;
            Template_parameters template_parameters;
        };

        struct Field {
            kieli::Name_lower name;
            Type              type;
            utl::Source_range source_range;
        };

        struct Struct_constructor {
            std::vector<Field> fields;
        };

        struct Tuple_constructor {
            std::vector<utl::Wrapper<Type>> types;
        };

        struct Unit_constructor {};

        using Constructor_body
            = std::variant<Struct_constructor, Tuple_constructor, Unit_constructor>;

        struct Constructor {
            kieli::Name_upper name;
            Constructor_body  body;
        };

        struct Enum {
            std::vector<Constructor> constructors;
            kieli::Name_upper        name;
            Template_parameters      template_parameters;
        };

        struct Alias {
            kieli::Name_upper   name;
            Type                type;
            Template_parameters template_parameters;
        };

        struct Typeclass {
            std::vector<Function_signature> function_signatures;
            std::vector<Type_signature>     type_signatures;
            kieli::Name_upper               name;
            Template_parameters             template_parameters;
        };

        struct Implementation {
            Type                    type;
            std::vector<Definition> definitions;
            Template_parameters     template_parameters;
        };

        struct Instantiation {
            Class_reference         typeclass;
            Type                    self_type;
            std::vector<Definition> definitions;
            Template_parameters     template_parameters;
        };

        struct Namespace {
            std::vector<Definition> definitions;
            kieli::Name_lower       name;
            Template_parameters     template_parameters;
        };
    } // namespace definition

    struct Definition {
        using Variant = std::variant<
            definition::Function,
            definition::Enum,
            definition::Alias,
            definition::Typeclass,
            definition::Implementation,
            definition::Instantiation,
            definition::Namespace>;

        Variant              value;
        utl::Source::Wrapper source;
        utl::Source_range    source_range;
    };

    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct [[nodiscard]] Module {
        std::vector<Definition> definitions;
        Node_arena              node_arena;
    };

    auto format_to(Wildcard const&, std::string&) -> void;
    auto format_to(Expression const&, std::string&) -> void;
    auto format_to(Pattern const&, std::string&) -> void;
    auto format_to(Type const&, std::string&) -> void;
    auto format_to(Definition const&, std::string&) -> void;
    auto format_to(Mutability const&, std::string&) -> void;
    auto format_to(Qualified_name const&, std::string&) -> void;
    auto format_to(Class_reference const&, std::string&) -> void;
    auto format_to(Function_parameter const&, std::string&) -> void;
    auto format_to(Function_argument const&, std::string&) -> void;
    auto format_to(Template_parameter const&, std::string&) -> void;
    auto format_to(Template_argument const&, std::string&) -> void;
    auto format_to(pattern::Field const&, std::string&) -> void;
    auto format_to(pattern::Constructor_body const&, std::string&) -> void;
    auto format_to(pattern::Constructor const&, std::string&) -> void;
    auto format_to(definition::Field const&, std::string&) -> void;
    auto format_to(definition::Constructor_body const&, std::string&) -> void;
    auto format_to(definition::Constructor const&, std::string&) -> void;
    auto format_to(definition::Template_parameters const&, std::string&) -> void;

    inline constexpr auto to_string = [](auto const& x) -> std::string
        requires requires(std::string out) { ast::format_to(x, out); }
    {
        std::string output;
        ast::format_to(x, output);
        return output;
    };

} // namespace ast
