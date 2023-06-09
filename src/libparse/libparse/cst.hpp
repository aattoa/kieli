#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <liblex/token.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


/*

    The Concrete Syntax Tree (CST) is the highest level structured representation
    of a program's syntax. It is produced by parsing a sequence of tokens. Any
    syntactically valid program can be represented as a CST, but such a program
    may still be erroneous in other ways, and such errors can only be revealed
    by subsequent compilation steps.

    For example, the following expression is syntactically valid, and
    can thus be represented by a CST node, but it will be rejected upon
    expression resolution due to the obvious type error:

        let x: Int = "hello"

*/


namespace cst {

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Definition;

    struct Token {
        utl::Source_view source_view;
        std::string_view preceding_trivia;
        std::string_view trailing_trivia;
    };

    struct Upper_name {
        compiler::Identifier identifier;
        utl::Source_view     source_view;
    };
    struct Lower_name {
        compiler::Identifier identifier;
        utl::Source_view     source_view;
    };

    template <class T>
    struct Comma_separated_syntax {
        struct Element {
            T                   value;
            tl::optional<Token> trailing_comma_token;
        };
        std::vector<Element> elements;
    };
    template <class T>
    struct Pipe_separated_syntax {
        struct Element {
            T                   value;
            tl::optional<Token> trailing_pipe_token;
        };
        std::vector<Element> elements;
    };
    template <class T>
    struct Plus_separated_syntax {
        struct Element {
            T                   value;
            tl::optional<Token> trailing_plus_token;
        };
        std::vector<Element> elements;
    };


    struct Lower_name_equals_syntax {
        Lower_name name;
        Token      equals_sign_token;
    };

    struct Type_annotation_syntax {
        utl::Wrapper<Type> type;
        Token              colon_token;
    };

    struct Mutability {
        struct Concrete {
            bool is_mutable = false;
        };
        struct Parameterized {
            compiler::Identifier identifier;
            Token                question_mark_token;
        };
        using Variant = std::variant<Concrete, Parameterized>;

        Variant          value;
        utl::Source_view source_view;
        Token            mut_keyword_token;
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
        Variant                  value;
        tl::optional<Lower_name> name;
    };

    struct Template_arguments {
        Comma_separated_syntax<Template_argument> arguments;
        Token                                     open_bracket_token;
        Token                                     close_bracket_token;
    };

    struct Qualifier {
        tl::optional<Template_arguments> template_arguments;
        Lower_name                       name;
        utl::Source_view                 source_view;
        Token                            preceding_double_colon_token;
    };

    struct Root_qualifier {
        struct Global {};
        using Variant = std::variant<
            Global,              // global::id
            utl::Wrapper<Type>>; // Type::id
        Variant value;
    };

    struct Qualified_name {
        std::vector<Qualifier>       middle_qualifiers;
        tl::optional<Root_qualifier> root_qualifier;
        Lower_name                   primary_name;

        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    struct Class_reference {
        tl::optional<Template_arguments> template_arguments;
        Qualified_name                   name;
        utl::Source_view                 source_view;
    };

    struct Function_parameter {
        struct Default_argument {
            utl::Wrapper<Expression> expression;
            Token                    equals_sign_token;
        };
        utl::Wrapper<Pattern>                pattern;
        tl::optional<Type_annotation_syntax> type;
        tl::optional<Default_argument>       default_argument;
    };

    struct Function_parameters {
        Comma_separated_syntax<Function_parameter> parameters;
        Token                                      open_parenthesis_token;
        Token                                      close_parenthesis_token;
    };

    struct Function_argument {
        tl::optional<Lower_name_equals_syntax> argument_name;
        utl::Wrapper<Expression>               expression;
    };

    struct Function_arguments {
        Comma_separated_syntax<Function_argument> arguments;
        Token                                     open_parenthesis_token;
        Token                                     close_parenthesis_token;
    };

    struct Template_parameter {
        struct Type {
            Plus_separated_syntax<Class_reference> classes;
            Upper_name                             name;
        };
        struct Value {
            tl::optional<Type_annotation_syntax> type;
            Lower_name                           name;
        };
        struct Mutability {
            Lower_name name;
        };
        using Variant = std::variant<Type, Value, Mutability>;

        Variant                         value;
        tl::optional<Template_argument> default_argument;
        utl::Source_view                source_view;
    };

    struct Template_parameters {
        Comma_separated_syntax<Template_parameter> parameters;
        Token                                      open_bracket_token;
        Token                                      close_bracket_token;
    };


    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
        struct Array_literal {
            Comma_separated_syntax<utl::Wrapper<Expression>> elements;
            Token                                            open_bracket_token;
            Token                                            close_bracket_token;
        };
        struct Self {};
        struct Variable {
            Qualified_name name;
        };
        struct Template_application {
            Template_arguments template_arguments;
            Qualified_name     name;
        };
        struct Tuple {
            Comma_separated_syntax<utl::Wrapper<Expression>> fields;
            Token                                            open_parenthesis_token;
            Token                                            close_parenthesis_token;
        };
        struct Block {
            struct Side_effect {
                Token                    trailing_semicolon_token;
                utl::Wrapper<Expression> expression;
            };
            std::vector<Side_effect>               side_effects;
            tl::optional<utl::Wrapper<Expression>> result_expression;
        };
        struct Invocation {
            Function_arguments       function_arguments;
            utl::Wrapper<Expression> function_expression;
        };
        struct Struct_initializer {
            struct Member_initializer {
                Lower_name               name;
                utl::Wrapper<Expression> expression;
                Token                    equals_sign_token;
            };
            std::vector<Member_initializer> member_initializers;
            utl::Wrapper<Type>              struct_type;
        };
        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left_operand;
            utl::Wrapper<Expression> right_operand;
            compiler::Operator       operator_name;
            Token                    operator_token;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            Lower_name               field_name;
            Token                    dot_token;
        };
        struct Tuple_field_access {
            utl::Wrapper<Expression> base_expression;
            utl::Usize               field_index {};
            utl::Source_view         field_index_source_view;
            Token                    dot_token;
        };
        struct Array_index_access {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Expression> index_expression;
        };
        struct Method_invocation {
            Function_arguments       function_arguments;
            Template_arguments       template_arguments;
            utl::Wrapper<Expression> base_expression;
            Lower_name               method_name;
        };
        struct Conditional {
            struct False_branch {
            };
            utl::Wrapper<Expression>   condition;
            utl::Wrapper<Expression>   true_branch;
            tl::optional<False_branch> false_branch;
            Token                      if_keyword_token;
        };
        struct Match {
            struct Case {
                Token                    arrow_token;
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };
            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };
        struct Type_cast {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Type>       target_type;
        };
        struct Type_ascription {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Type>       ascribed_type;
        };
        struct Let_binding {
            utl::Wrapper<Pattern>                pattern;
            tl::optional<Type_annotation_syntax> type;
            utl::Wrapper<Expression>             initializer;
            Token                                let_keyword_token;
            Token                                equals_sign_token;
        };
        struct Conditional_let {
            utl::Wrapper<Pattern>    pattern;
            utl::Wrapper<Expression> initializer;
        };
        struct Local_type_alias {
            Upper_name         alias_name;
            utl::Wrapper<Type> aliased_type;
            Token              alias_keyword_token;
            Token              equals_sign_token;
        };
        struct Infinite_loop {
            utl::Wrapper<Expression> body;
            Token                    loop_keyword_token;
        };
        struct While_loop {
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> body;
            Token                    while_keyword_token;
        };
        struct For_loop {
            utl::Wrapper<Pattern>    iterator;
            utl::Wrapper<Expression> iterable;
            utl::Wrapper<Expression> body;
            Token                    for_keyword_token;
            Token                    in_keyword_token;
        };
        struct Continue {};
        struct Break {
            tl::optional<utl::Wrapper<Expression>> result;
            Token                                  break_keyword_token;
        };
        struct Discard {
            utl::Wrapper<Expression> discarded_expression;
            Token                    discard_keyword_token;
        };
        struct Ret {
            tl::optional<utl::Wrapper<Expression>> returned_expression;
            Token                                  ret_keyword_token;
        };
        struct Sizeof {
            utl::Wrapper<Type> inspected_type;
            Token              sizeof_keyword_token;
            Token              open_parenthesis_token;
            Token              close_parenthesis_token;
        };
        struct Reference {
            tl::optional<Mutability> mutability;
            utl::Wrapper<Expression> referenced_expression;
            Token                    ampersand_token;
        };
        struct Addressof {
            utl::Wrapper<Expression> lvalue_expression;
            Token                    addressof_keyword_token;
            Token                    open_parenthesis_token;
            Token                    close_parenthesis_token;
        };
        struct Reference_dereference {
            utl::Wrapper<Expression> dereferenced_expression;
        };
        struct Pointer_dereference {
            utl::Wrapper<Expression> pointer_expression;
            Token                    dereference_keyword_token;
            Token                    open_parenthesis_token;
            Token                    close_parenthesis_token;
        };
        struct Unsafe {
            Token                    unsafe_keyword_token;
            utl::Wrapper<Expression> expression;
        };
        struct Move {
            utl::Wrapper<Expression> lvalue;
            Token                    mov_keyword_token;
        };
        struct Meta {
            utl::Wrapper<Expression> expression;
            Token                    meta_keyword_token;
        };
        struct Hole {};
    }

    struct Expression {
        using Variant = std::variant<
            expression::Literal<kieli::Integer>,
            expression::Literal<kieli::Floating>,
            expression::Literal<kieli::Character>,
            expression::Literal<kieli::Boolean>,
            expression::Literal<compiler::String>,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Template_application,
            expression::Tuple,
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
            expression::Type_cast,
            expression::Type_ascription,
            expression::Let_binding,
            expression::Conditional_let,
            expression::Local_type_alias,
            expression::Infinite_loop,
            expression::While_loop,
            expression::For_loop,
            expression::Continue,
            expression::Break,
            expression::Discard,
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
            compiler::Identifier identifier;
            Mutability           mutability;
        };
        struct Constructor {
            Qualified_name                      constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Abbreviated_constructor {
            Lower_name                          constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
            Token                               double_colon_token;
        };
        struct Tuple {
            Comma_separated_syntax<utl::Wrapper<Pattern>> patterns;
            Token                                         open_parenthesis_token;
            Token                                         close_parenthesis_token;
        };
        struct Slice {
            Comma_separated_syntax<utl::Wrapper<Pattern>> patterns;
            Token                                         open_bracket_token;
            Token                                         close_bracket_token;
        };
        struct Alias {
            utl::Wrapper<Pattern> aliased_pattern;
            Name                  alias_name;
            Token                 as_keyword_token;
        };
        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression            guard;
            Token                 if_keyword_token;
        };
    }

    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<kieli::Integer>,
            pattern::Literal<kieli::Floating>,
            pattern::Literal<kieli::Character>,
            pattern::Literal<kieli::Boolean>,
            pattern::Literal<compiler::String>,
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
        enum class Integer {
            i8, i16, i32, i64,
            u8, u16, u32, u64,
            _enumerator_count,
        };
        struct Floating {};
        struct Character {};
        struct Boolean {};
        struct String {};
        struct Wildcard {};
        struct Self {};
        struct Typename {
            Qualified_name name;
        };
        struct Tuple {
            Comma_separated_syntax<utl::Wrapper<Type>> field_types;
            Token                                      open_parenthesis_token;
            Token                                      close_parenthesis_token;
        };
        struct Array {
            utl::Wrapper<Type>       element_type;
            utl::Wrapper<Expression> array_length;
            Token                    open_bracket_token;
            Token                    close_bracket_token;
            Token                    semicolon_token;
        };
        struct Slice {
            utl::Wrapper<Type> element_type;
            Token              open_bracket_token;
            Token              close_bracket_token;
        };
        struct Function {
            Comma_separated_syntax<utl::Wrapper<Expression>> parameter_types;
            Type_annotation_syntax                           return_type;
            Token                                            fn_keyword_token;
            Token                                            open_parenthesis_token;
            Token                                            close_parenthesis_token;
        };
        struct Typeof {
            utl::Wrapper<Expression> inspected_expression;
            Token                    typeof_keyword_token;
        };
        struct Reference {
            utl::Wrapper<Type>       referenced_type;
            tl::optional<Mutability> mutability;
            Token                    ampersand_token;
        };
        struct Pointer {
            utl::Wrapper<Type>       pointed_to_type;
            tl::optional<Mutability> mutability;
            Token                    asterisk_token;
        };
        struct Instance_of {
            std::vector<Class_reference> classes;
            Token                        inst_keyword_token;
        };
        struct Template_application {
            Template_arguments template_arguments;
            Qualified_name     name;
        };
    }

    struct Type {
        using Variant = std::variant<
            type::Integer,
            type::Floating,
            type::Character,
            type::Boolean,
            type::String,
            type::Wildcard,
            type::Self,
            type::Typename,
            type::Tuple,
            type::Array,
            type::Slice,
            type::Function,
            type::Typeof,
            type::Instance_of,
            type::Reference,
            type::Pointer,
            type::Template_application>;

        Variant          value;
        utl::Source_view source_view;
    };



    struct Self_parameter {
        Mutability          mutability;
        tl::optional<Token> ampersand_token;
        Token               self_keyword_token;
        utl::Source_view    source_view;

        [[nodiscard]] auto is_reference() const noexcept -> bool;
    };

    struct Function_signature {
        tl::optional<Template_parameters> template_parameters;
        Function_parameters               parameters;
        tl::optional<Self_parameter>      self_parameter;
        Type_annotation_syntax            return_type;
        Lower_name                        name;
        Token                             fn_keyword_token;
    };

    struct Type_signature {
        tl::optional<Template_parameters> template_parameters;
        std::vector<Class_reference>      classes;
        Upper_name                        name;
    };



    namespace definition {
        struct Function {
            Function_signature       signature;
            utl::Wrapper<Expression> body;
            Token                    fn_keyword_token;
        };
        struct Struct {
            struct Member {
                Lower_name             name;
                Type_annotation_syntax type;
                utl::Strong<bool>      is_public;
                utl::Source_view       source_view;
            };
            tl::optional<Template_parameters> template_parameters;
            Comma_separated_syntax<Member>    members;
            Upper_name                        name;
            Token                             struct_keyword_token;
        };
        struct Enum {
            struct Constructor {
                Lower_name                       name;
                tl::optional<utl::Wrapper<Type>> payload_type;
                utl::Source_view                 source_view;
                Token                            open_parenthesis_token;
                Token                            close_parenthesis_token;
            };
            tl::optional<Template_parameters>  template_parameters;
            Pipe_separated_syntax<Constructor> constructors;
            Upper_name                         name;
            Token                              enum_keyword_token;
            Token                              equals_sign_token;
        };
        struct Alias {
            tl::optional<Template_parameters> template_parameters;
            Upper_name                        name;
            utl::Wrapper<Type>                type;
            Token                             alias_keyword_token;
            Token                             equals_sign_token;
        };
        struct Typeclass {
            tl::optional<Template_parameters> template_parameters;
            std::vector<Function_signature>   function_signatures;
            std::vector<Type_signature>       type_signatures;
            Upper_name                        name;
            Token                             class_keyword_token;
        };
        struct Implementation {
            tl::optional<Template_parameters> template_parameters;
            std::vector<Definition>           definitions;
            utl::Wrapper<Type>                self_type;
            Token                             impl_keyword_token;
        };
        struct Instantiation {
            tl::optional<Template_parameters> template_parameters;
            Class_reference                   typeclass;
            std::vector<Definition>           definitions;
            utl::Wrapper<Type>                self_type;
            Token                             inst_keyword_token;
            Token                             for_keyword_token;
        };
        struct Namespace {
            tl::optional<Template_parameters> template_parameters;
            std::vector<Definition>           definitions;
            Lower_name                        name;
            Token                             namespace_keyword_token;
        };
    }

    struct Definition {
        using Variant = std::variant<
            definition::Function,
            definition::Struct,
            definition::Enum,
            definition::Alias,
            definition::Typeclass,
            definition::Implementation,
            definition::Instantiation,
            definition::Namespace>;

        Variant          value;
        utl::Source_view source_view;
    };

}
