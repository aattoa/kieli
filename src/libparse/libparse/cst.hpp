#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <liblex/token.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


/*

    The Concrete Syntax Tree (CST) is the highest level structured representation
    of a program's syntax. It is produced by parsing a sequence of tokens. Any
    syntactically valid program can be represented as a CST, but such a program
    may still be erroneous in other ways, and such errors can only be revealed
    by subsequent compilation steps.

    For example, the following expression is syntactically valid, and can thus
    be represented by a CST node, but it will be rejected upon expression
    resolution due to the obvious type error:

        let x: Int = "hello"

*/


namespace cst {

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Definition;

    struct [[nodiscard]] Token {
        utl::Source_view source_view;
        std::string_view preceding_trivia;
        std::string_view trailing_trivia;

        // Precondition: `pointer` must point to a token within a contiguous token sequence
        static auto from_lexical(kieli::Lexical_token const* pointer) -> Token;
    };

    template <class T>
    struct Surrounded {
        T     value;
        Token open_token;
        Token close_token;
    };

    template <class T>
    struct Separated_sequence {
        std::vector<T>     elements;
        std::vector<Token> separator_tokens;
    };

    struct Name_lower_equals {
        compiler::Name_lower name;
        Token                equals_sign_token;
    };

    struct Type_annotation {
        utl::Wrapper<Type> type;
        Token              colon_token;
    };

    struct Mutability {
        struct Concrete {
            bool is_mutable = false;
        };
        struct Parameterized {
            compiler::Name_lower name;
            Token                question_mark_token;
        };
        using Variant = std::variant<Concrete, Parameterized>;

        Variant          value;
        utl::Source_view source_view;
        Token            mut_or_immut_keyword_token;
    };

    struct Self_parameter {
        tl::optional<Mutability> mutability;
        tl::optional<Token>      ampersand_token;
        Token                    self_keyword_token;
        utl::Source_view         source_view;

        [[nodiscard]] auto is_reference() const noexcept -> bool;
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

        auto source_view() const -> utl::Source_view;
    };

    using Template_arguments = Surrounded<Separated_sequence<Template_argument>>;

    struct Qualifier {
        tl::optional<Template_arguments> template_arguments;
        compiler::Name_dynamic           name;
        tl::optional<Token>              trailing_double_colon_token;
        utl::Source_view                 source_view;
    };

    struct Root_qualifier {
        struct Global {};
        using Variant = std::variant<
            Global,              // global::id
            utl::Wrapper<Type>>; // Type::id
        Variant value;
        Token   double_colon_token;
    };

    struct Qualified_name {
        Separated_sequence<Qualifier> middle_qualifiers;
        tl::optional<Root_qualifier>  root_qualifier;
        compiler::Name_dynamic        primary_name;

        [[nodiscard]] auto is_upper()       const noexcept -> bool;
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
        utl::Wrapper<Pattern>          pattern;
        tl::optional<Type_annotation>  type;
        tl::optional<Default_argument> default_argument;
    };

    struct Function_parameters {
        Separated_sequence<Function_parameter> normal_parameters;
        tl::optional<cst::Self_parameter>      self_parameter;
        tl::optional<Token>                    comma_token_after_self_parameter;
        Token                                  open_parenthesis_token;
        Token                                  close_parenthesis_token;
    };

    struct Function_argument {
        tl::optional<Name_lower_equals> argument_name;
        utl::Wrapper<Expression>        expression;
    };

    using Function_arguments = Surrounded<Separated_sequence<Function_argument>>;

    struct Template_parameter {
        struct Type_parameter {
            Separated_sequence<Class_reference> classes;
            compiler::Name_upper                name;
        };
        struct Value_parameter {
            tl::optional<utl::Wrapper<Type>> type;
            compiler::Name_lower             name;
        };
        struct Mutability_parameter {
            compiler::Name_lower name;
            Token                mut_keyword_token;
        };
        using Variant = std::variant<Type_parameter, Value_parameter, Mutability_parameter>;

        struct Default_argument {
            Template_argument argument;
            Token             equals_sign_token;
        };

        Variant                        value;
        tl::optional<Token>            colon_token;
        tl::optional<Default_argument> default_argument;
        utl::Source_view               source_view;
    };

    using Template_parameters = Surrounded<Separated_sequence<Template_parameter>>;

    namespace expression {
        struct Parenthesized {
            Surrounded<utl::Wrapper<Expression>> expression;
        };
        struct Array_literal {
            Surrounded<Separated_sequence<utl::Wrapper<Expression>>> elements;
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
            Surrounded<Separated_sequence<utl::Wrapper<Expression>>> fields;
        };
        struct Block {
            struct Side_effect {
                utl::Wrapper<Expression> expression;
                Token                    trailing_semicolon_token;
            };
            std::vector<Side_effect>               side_effects;
            tl::optional<utl::Wrapper<Expression>> result_expression;
            Token                                  open_brace_token;
            Token                                  close_brace_token;
        };
        struct Invocation {
            Function_arguments       function_arguments;
            utl::Wrapper<Expression> function_expression;
        };
        struct Struct_initializer {
            struct Member_initializer {
                compiler::Name_lower     name;
                utl::Wrapper<Expression> expression;
                Token                    equals_sign_token;
            };
            Surrounded<Separated_sequence<Member_initializer>> member_initializers;
            utl::Wrapper<Type>                                 struct_type;
        };
        struct Binary_operator_invocation_sequence {
            struct Operator_and_operand {
                utl::Pooled_string       operator_name;
                Token                    operator_token;
                utl::Wrapper<Expression> right_operand;
            };
            std::vector<Operator_and_operand> sequence_tail;
            utl::Wrapper<Expression>          leftmost_operand;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            compiler::Name_lower     field_name;
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
            Token                    dot_token;
        };
        struct Method_invocation {
            Function_arguments               function_arguments;
            tl::optional<Template_arguments> template_arguments;
            utl::Wrapper<Expression>         base_expression;
            compiler::Name_lower             method_name;
        };
        struct Conditional {
            struct False_branch {
                utl::Wrapper<Expression> body;
                Token                    else_or_elif_keyword_token;
            };
            utl::Wrapper<Expression>   condition;
            utl::Wrapper<Expression>   true_branch;
            tl::optional<False_branch> false_branch;
            Token                      if_or_elif_keyword_token;
            utl::Explicit<bool>        is_elif_conditional;
        };
        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
                Token                    arrow_token;
                tl::optional<Token>      optional_semicolon_token;
            };
            Surrounded<std::vector<Case>> cases;
            utl::Wrapper<Expression>      matched_expression;
            Token                         match_keyword_token;
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
            utl::Wrapper<Pattern>         pattern;
            tl::optional<Type_annotation> type;
            utl::Wrapper<Expression>      initializer;
            Token                         let_keyword_token;
            Token                         equals_sign_token;
        };
        struct Conditional_let {
            utl::Wrapper<Pattern>    pattern;
            utl::Wrapper<Expression> initializer;
            Token                    let_keyword_token;
            Token                    equals_sign_token;
        };
        struct Local_type_alias {
            compiler::Name_upper alias_name;
            utl::Wrapper<Type>   aliased_type;
            Token                alias_keyword_token;
            Token                equals_sign_token;
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
        struct Continue {
            Token continue_keyword_token;
        };
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
        struct Reference {
            tl::optional<Mutability> mutability;
            utl::Wrapper<Expression> referenced_expression;
            Token                    ampersand_token;
        };
        struct Sizeof {
            Surrounded<utl::Wrapper<Type>> inspected_type;
            Token                          sizeof_keyword_token;
        };
        struct Addressof {
            Surrounded<utl::Wrapper<Expression>> lvalue_expression;
            Token                                addressof_keyword_token;
        };
        struct Reference_dereference {
            utl::Wrapper<Expression> dereferenced_expression;
            Token                    asterisk_token;
        };
        struct Pointer_dereference {
            Surrounded<utl::Wrapper<Expression>> pointer_expression;
            Token                                dereference_keyword_token;
        };
        struct Unsafe {
            utl::Wrapper<Expression> expression;
            Token                    unsafe_keyword_token;
        };
        struct Move {
            utl::Wrapper<Expression> lvalue;
            Token                    mov_keyword_token;
        };
        struct Meta {
            Surrounded<utl::Wrapper<Expression>> expression;
            Token                                meta_keyword_token;
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
            expression::Parenthesized,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Template_application,
            expression::Tuple,
            expression::Block,
            expression::Invocation,
            expression::Struct_initializer,
            expression::Binary_operator_invocation_sequence,
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
        struct Parenthesized {
            Surrounded<utl::Wrapper<Pattern>> pattern;
        };
        struct Wildcard {};
        struct Name {
            compiler::Name_lower     name;
            tl::optional<Mutability> mutability;
        };
        struct Constructor {
            Qualified_name                                       constructor_name;
            tl::optional<cst::Surrounded<utl::Wrapper<Pattern>>> payload_pattern;
        };
        struct Abbreviated_constructor {
            compiler::Name_lower                                 constructor_name;
            tl::optional<cst::Surrounded<utl::Wrapper<Pattern>>> payload_pattern;
            Token                                                double_colon_token;
        };
        struct Tuple {
            Surrounded<Separated_sequence<utl::Wrapper<Pattern>>> patterns;
        };
        struct Top_level_tuple {
            Separated_sequence<utl::Wrapper<Pattern>> patterns;
        };
        struct Slice {
            Surrounded<Separated_sequence<utl::Wrapper<Pattern>>> patterns;
        };
        struct Alias {
            compiler::Name_lower     alias_name;
            tl::optional<Mutability> alias_mutability;
            utl::Wrapper<Pattern>    aliased_pattern;
            Token                    as_keyword_token;
        };
        struct Guarded {
            utl::Wrapper<Pattern>    guarded_pattern;
            utl::Wrapper<Expression> guard_expression;
            Token                    if_keyword_token;
        };
    }

    struct Pattern {
        using Variant = std::variant<
            pattern::Parenthesized,
            compiler::Integer,
            compiler::Floating,
            compiler::Character,
            compiler::Boolean,
            compiler::String,
            pattern::Wildcard,
            pattern::Name,
            pattern::Constructor,
            pattern::Abbreviated_constructor,
            pattern::Tuple,
            pattern::Top_level_tuple,
            pattern::Slice,
            pattern::Alias,
            pattern::Guarded>;

        Variant          value;
        utl::Source_view source_view;
    };



    namespace type {
        struct Parenthesized {
            Surrounded<utl::Wrapper<Type>> type;
        };
        struct Wildcard {};
        struct Self {};
        struct Typename {
            Qualified_name name;
        };
        struct Tuple {
            Surrounded<Separated_sequence<utl::Wrapper<Type>>> field_types;
        };
        struct Array {
            utl::Wrapper<Type>       element_type;
            utl::Wrapper<Expression> array_length;
            Token                    open_bracket_token;
            Token                    close_bracket_token;
            Token                    semicolon_token;
        };
        struct Slice {
            Surrounded<utl::Wrapper<Type>> element_type;
        };
        struct Function {
            Surrounded<Separated_sequence<utl::Wrapper<Type>>> parameter_types;
            Type_annotation                                    return_type;
            Token                                              fn_keyword_token;
        };
        struct Typeof {
            Surrounded<utl::Wrapper<Expression>> inspected_expression;
            Token                                typeof_keyword_token;
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
            Separated_sequence<Class_reference> classes;
            Token                               inst_keyword_token;
        };
        struct Template_application {
            Template_arguments template_arguments;
            Qualified_name     name;
        };
    }

    struct Type {
        using Variant = std::variant<
            type::Parenthesized,
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
            type::Instance_of,
            type::Reference,
            type::Pointer,
            type::Template_application>;

        Variant          value;
        utl::Source_view source_view;
    };


    struct Function_signature {
        tl::optional<Template_parameters> template_parameters;
        Function_parameters               function_parameters;
        tl::optional<Type_annotation>     return_type;
        compiler::Name_lower              name;
        Token                             fn_keyword_token;
    };

    struct Type_signature {
        tl::optional<Template_parameters>   template_parameters;
        Separated_sequence<Class_reference> classes;
        compiler::Name_upper                name;
        tl::optional<Token>                 classes_colon_token;
        Token                               alias_keyword_token;
    };



    namespace definition {
        struct Function {
            Function_signature       signature;
            utl::Wrapper<Expression> body;
            tl::optional<Token>      optional_equals_sign_token;
            Token                    fn_keyword_token;
        };
        struct Struct {
            struct Member {
                compiler::Name_lower name;
                Type_annotation      type;
                utl::Explicit<bool>  is_public;
                utl::Source_view     source_view;
            };
            tl::optional<Template_parameters> template_parameters;
            Separated_sequence<Member>        members;
            compiler::Name_upper              name;
            Token                             struct_keyword_token;
            Token                             equals_sign_token;
        };
        struct Enum {
            struct Constructor {
                tl::optional<Surrounded<Separated_sequence<utl::Wrapper<cst::Type>>>> payload_types;
                compiler::Name_lower                                                            name;
                utl::Source_view                                                      source_view;
            };
            tl::optional<Template_parameters> template_parameters;
            Separated_sequence<Constructor>   constructors;
            compiler::Name_upper              name;
            Token                             enum_keyword_token;
            Token                             equals_sign_token;
        };
        struct Alias {
            tl::optional<Template_parameters> template_parameters;
            compiler::Name_upper              name;
            utl::Wrapper<Type>                type;
            Token                             alias_keyword_token;
            Token                             equals_sign_token;
        };
        struct Typeclass {
            tl::optional<Template_parameters> template_parameters;
            std::vector<Function_signature>   function_signatures;
            std::vector<Type_signature>       type_signatures;
            compiler::Name_upper              name;
            Token                             class_keyword_token;
            Token                             open_brace_token;
            Token                             close_brace_token;
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
            compiler::Name_lower              name;
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


    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct [[nodiscard]] Module {
        std::vector<Definition>          definitions;
        tl::optional<utl::Pooled_string> name;
        std::vector<utl::Pooled_string>  imports;
    };

}
