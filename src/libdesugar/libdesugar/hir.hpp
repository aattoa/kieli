#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


/*

    The Abstract Syntax Tree (AST) is a high level structured representation
    of a program's syntax, much like the CST, just without the exact source
    information. It is produced by desugaring the CST.

    For example, the following CST node:
        while a { b }

    would be desugared to the following AST node:
        loop { if a { b } else { break } }

*/


namespace hir {
    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Definition;


    struct Mutability {
        struct Concrete {
            utl::Explicit<bool> is_mutable;
        };
        struct Parameterized {
            compiler::Name_lower name;
        };
        using Variant = std::variant<Concrete, Parameterized>;

        Variant             value;
        utl::Explicit<bool> is_explicit;
        utl::Source_view    source_view;
    };


    struct Template_argument {
        struct Wildcard {
            utl::Source_view source_view;
        };
        using Variant = std::variant<
            utl::Wrapper<Type>,
            utl::Wrapper<Expression>,
            Mutability,
            Wildcard>;
        Variant value;
    };

    struct Qualifier {
        tl::optional<std::vector<Template_argument>> template_arguments;
        compiler::Name_dynamic                       name;
        utl::Source_view                             source_view;
    };

    struct Root_qualifier {
        struct Global {};
        using Variant = std::variant<Global, utl::Wrapper<Type>>;
        Variant value;
    };

    struct Qualified_name {
        std::vector<Qualifier>       middle_qualifiers;
        tl::optional<Root_qualifier> root_qualifier;
        compiler::Name_dynamic       primary_name;

        [[nodiscard]] auto is_upper      () const noexcept -> bool;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    struct Class_reference {
        tl::optional<std::vector<Template_argument>> template_arguments;
        Qualified_name                               name;
        utl::Source_view                             source_view;
    };

    struct Template_parameter {
        struct Type_parameter {
            std::vector<Class_reference> classes;
            compiler::Name_upper         name;
        };
        struct Value_parameter {
            tl::optional<utl::Wrapper<Type>> type;
            compiler::Name_lower             name;
        };
        struct Mutability_parameter {
            compiler::Name_lower name;
        };

        using Variant = std::variant<
            Type_parameter,
            Value_parameter,
            Mutability_parameter>;

        Variant                         value;
        tl::optional<Template_argument> default_argument;
        utl::Source_view                source_view;
    };

    struct Function_argument {
        utl::Wrapper<Expression>           expression;
        tl::optional<compiler::Name_lower> argument_name;
    };

    struct Function_parameter {
        utl::Wrapper<Pattern>                  pattern;
        tl::optional<utl::Wrapper<Type>>       type;
        tl::optional<utl::Wrapper<Expression>> default_argument;
    };


    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
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
        struct Struct_initializer {
            utl::Flatmap<compiler::Name_lower, utl::Wrapper<Expression>> member_initializers;
            utl::Wrapper<Type>                                           struct_type;
        };
        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left;
            utl::Wrapper<Expression> right;
            utl::Pooled_string       op;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            compiler::Name_lower     field_name;
        };
        struct Tuple_field_access {
            utl::Wrapper<Expression>  base_expression;
            utl::Explicit<utl::Usize> field_index;
            utl::Source_view          field_index_source_view;
        };
        struct Array_index_access {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Expression> index_expression;
        };
        struct Method_invocation {
            std::vector<Function_argument>               arguments;
            tl::optional<std::vector<Template_argument>> template_arguments;
            utl::Wrapper<Expression>                     base_expression;
            compiler::Name_lower                         method_name;
        };
        struct Conditional {
            enum class Source { normal_conditional, while_loop_body };
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
            utl::Wrapper<Pattern>            pattern;
            utl::Wrapper<Expression>         initializer;
            tl::optional<utl::Wrapper<Type>> type;
        };
        struct Local_type_alias {
            compiler::Name_upper alias_name;
            utl::Wrapper<Type>   aliased_type;
        };
        struct Ret {
            tl::optional<utl::Wrapper<Expression>> returned_expression;
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
    }

    struct Expression {
        using Variant = std::variant<
            expression::Literal<compiler::Integer>,
            expression::Literal<compiler::Floating>,
            expression::Literal<compiler::Character>,
            expression::Literal<compiler::Boolean>,
            expression::Literal<utl::Pooled_string>,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Block,
            expression::Invocation,
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

        Variant          value;
        utl::Source_view source_view;
    };



    namespace pattern {
        template <class T>
        struct Literal {
            T value;
        };
        struct Wildcard {};
        struct Name {
            compiler::Name_lower name;
            Mutability           mutability;
        };
        struct Constructor {
            Qualified_name                      constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Abbreviated_constructor {
            compiler::Name_lower                constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Slice {
            std::vector<Pattern> element_patterns;
        };
        struct Alias {
            compiler::Name_lower  alias_name;
            Mutability            alias_mutability;
            utl::Wrapper<Pattern> aliased_pattern;
        };
        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression            guard;
        };
    }

    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<compiler::Integer>,
            pattern::Literal<compiler::Floating>,
            pattern::Literal<compiler::Character>,
            pattern::Literal<compiler::Boolean>,
            pattern::Literal<utl::Pooled_string>,
            pattern::Wildcard,
            pattern::Name,
            pattern::Constructor,
            pattern::Abbreviated_constructor,
            pattern::Tuple,
            pattern::Slice,
            pattern::Alias,
            pattern::Guarded>;

        Variant          value;
        utl::Source_view source_view;
    };



    namespace type {
        struct Wildcard {};
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
    }

    struct Type {
        using Variant = std::variant<
            compiler::built_in_type::Integer,
            compiler::built_in_type::Floating,
            compiler::built_in_type::Character,
            compiler::built_in_type::Boolean,
            compiler::built_in_type::String,
            type::Wildcard,
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

        Variant          value;
        utl::Source_view source_view;
    };


    struct Self_parameter {
        Mutability          mutability;
        utl::Explicit<bool> is_reference;
        utl::Source_view    source_view;
    };

    struct Function_signature {
        std::vector<Template_parameter> template_parameters;
        std::vector<Function_parameter> function_parameters;
        tl::optional<Self_parameter>    self_parameter;
        tl::optional<Type>              return_type;
        compiler::Name_lower            name;
    };

    struct Type_signature {
        std::vector<Template_parameter> template_parameters;
        std::vector<Class_reference>    classes;
        compiler::Name_upper            name;
    };


    namespace definition {
        struct Function {
            Function_signature signature;
            Expression         body;
        };
        struct Struct {
            struct Member {
                compiler::Name_lower name;
                Type                 type;
                utl::Explicit<bool>  is_public;
                utl::Source_view     source_view;
            };
            std::vector<Member>  members;
            compiler::Name_upper name;
        };
        struct Enum {
            struct Constructor {
                compiler::Name_lower            name;
                tl::optional<std::vector<Type>> payload_types;
                utl::Source_view                source_view;
            };
            std::vector<Constructor> constructors;
            compiler::Name_upper     name;
        };
        struct Alias {
            compiler::Name_upper name;
            Type                 type;
        };
        struct Typeclass {
            std::vector<Function_signature> function_signatures;
            std::vector<Type_signature>     type_signatures;
            compiler::Name_upper            name;
        };
        struct Implementation {
            Type                    type;
            std::vector<Definition> definitions;
        };
        struct Instantiation {
            Class_reference         typeclass;
            Type                    self_type;
            std::vector<Definition> definitions;
        };
        struct Namespace {
            std::vector<Definition> definitions;
            compiler::Name_lower    name;
        };

        template <class T>
        struct Template {
            T                               definition;
            std::vector<Template_parameter> parameters;
        };
        using Struct_template         = Template<Struct>;
        using Enum_template           = Template<Enum>;
        using Alias_template          = Template<Alias>;
        using Typeclass_template      = Template<Typeclass>;
        using Implementation_template = Template<Implementation>;
        using Instantiation_template  = Template<Instantiation>;
        using Namespace_template      = Template<Namespace>;
    }

    struct Definition {
        using Variant = std::variant<
            definition::Function,
            definition::Struct,
            definition::Struct_template,
            definition::Enum,
            definition::Enum_template,
            definition::Alias,
            definition::Alias_template,
            definition::Typeclass,
            definition::Typeclass_template,
            definition::Implementation,
            definition::Implementation_template,
            definition::Instantiation,
            definition::Instantiation_template,
            definition::Namespace,
            definition::Namespace_template>;

        Variant          value;
        utl::Source_view source_view;
    };


    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct Module {
        std::vector<Definition> definitions;
    };


    auto format_to(hir::Expression         const&, std::string&) -> void;
    auto format_to(hir::Pattern            const&, std::string&) -> void;
    auto format_to(hir::Type               const&, std::string&) -> void;
    auto format_to(hir::Mutability         const&, std::string&) -> void;
    auto format_to(hir::Qualified_name     const&, std::string&) -> void;
    auto format_to(hir::Class_reference    const&, std::string&) -> void;
    auto format_to(hir::Template_parameter const&, std::string&) -> void;
    auto format_to(hir::Template_argument  const&, std::string&) -> void;

    auto to_string(auto const& x) -> std::string
        requires requires { hir::format_to(x, std::declval<std::string&>()); }
    {
        std::string output;
        hir::format_to(x, output);
        return output;
    }

}
