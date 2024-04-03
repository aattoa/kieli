#pragma once

#include <libphase/phase.hpp>
#include <liblex/token.hpp>
#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>

/*

    The Concrete Syntax Tree (CST) is the highest level structured
    representation of a program's syntax. It is produced by parsing a sequence of
    tokens. Any syntactically valid program can be represented as a CST, but such
    a program may still be erroneous in other ways, and such errors can only be
    revealed by subsequent compilation steps.

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
        utl::Source_range source_range;
        std::string_view  preceding_trivia;

        // TODO: std::string_view succeeding_trivia

        static auto from_lexical(kieli::Token const&) -> Token;
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
        kieli::Name_lower name;
        Token             equals_sign_token;
    };

    struct Type_annotation {
        utl::Wrapper<Type> type;
        Token              colon_token;
    };

    struct Wildcard {
        utl::Source_range source_range;
    };

    namespace mutability {
        using Concrete = kieli::Mutability;

        struct Parameterized {
            kieli::Name_lower name;
            Token             question_mark_token;
        };
    } // namespace mutability

    struct Mutability {
        using Variant = std::variant<mutability::Concrete, mutability::Parameterized>;
        Variant           variant;
        utl::Source_range source_range;
        Token             mut_or_immut_keyword_token;
    };

    struct Self_parameter {
        std::optional<Mutability> mutability;
        std::optional<Token>      ampersand_token;
        Token                     self_keyword_token;
        utl::Source_range         source_range;

        [[nodiscard]] auto is_reference() const noexcept -> bool;
    };

    using Template_argument = std::variant< //
        utl::Wrapper<Type>,
        utl::Wrapper<Expression>,
        Mutability,
        Wildcard>;

    using Template_arguments = Surrounded<Separated_sequence<Template_argument>>;

    struct Qualifier {
        std::optional<Template_arguments> template_arguments;
        kieli::Name_dynamic               name;
        std::optional<Token>              trailing_double_colon_token;
        utl::Source_range                 source_range;
    };

    struct Global_root_qualifier {
        Token global_keyword;
    };

    struct Root_qualifier {
        std::variant<Global_root_qualifier, utl::Wrapper<Type>> variant;
        Token                                                   double_colon_token;
        utl::Source_range                                       source_range;
    };

    struct Qualified_name {
        Separated_sequence<Qualifier> middle_qualifiers;
        std::optional<Root_qualifier> root_qualifier;
        kieli::Name_dynamic           primary_name;
        utl::Source_range             source_range;

        [[nodiscard]] auto is_upper() const noexcept -> bool;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    struct Class_reference {
        std::optional<Template_arguments> template_arguments;
        Qualified_name                    name;
        utl::Source_range                 source_range;
    };

    template <class T>
    struct Default_argument {
        Token                     equals_sign_token;
        std::variant<T, Wildcard> variant;
    };

    using Type_parameter_default_argument       = Default_argument<utl::Wrapper<Type>>;
    using Value_parameter_default_argument      = Default_argument<utl::Wrapper<Expression>>;
    using Mutability_parameter_default_argument = Default_argument<Mutability>;

    struct Function_parameter {
        utl::Wrapper<Pattern>                           pattern;
        std::optional<Type_annotation>                  type;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    struct Function_parameters {
        Separated_sequence<Function_parameter> normal_parameters;
        std::optional<Self_parameter>          self_parameter;
        std::optional<Token>                   comma_token_after_self;
    };

    struct Function_argument {
        std::optional<Name_lower_equals> name;
        utl::Wrapper<Expression>         expression;
    };

    using Function_arguments = Surrounded<Separated_sequence<Function_argument>>;

    struct Template_type_parameter {
        kieli::Name_upper                              name;
        std::optional<Token>                           colon_token;
        Separated_sequence<Class_reference>            classes;
        std::optional<Type_parameter_default_argument> default_argument;
    };

    struct Template_value_parameter {
        kieli::Name_lower                               name;
        std::optional<Type_annotation>                  type_annotation;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    struct Template_mutability_parameter {
        kieli::Name_lower                                    name;
        Token                                                colon_token;
        Token                                                mut_keyword_token;
        std::optional<Mutability_parameter_default_argument> default_argument;
    };

    struct Template_parameter {
        using Variant = std::variant<
            Template_type_parameter,
            Template_value_parameter,
            Template_mutability_parameter>;

        Variant           variant;
        utl::Source_range source_range;
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

            std::vector<Side_effect>                side_effects;
            std::optional<utl::Wrapper<Expression>> result_expression;
            Token                                   open_brace_token;
            Token                                   close_brace_token;
        };

        struct Invocation {
            Function_arguments       function_arguments;
            utl::Wrapper<Expression> function_expression;
        };

        struct Unit_initializer {
            Qualified_name constructor;
        };

        struct Tuple_initializer {
            Qualified_name                                           constructor;
            Surrounded<Separated_sequence<utl::Wrapper<Expression>>> initializers;
        };

        struct Struct_initializer {
            struct Field {
                kieli::Name_lower        name;
                Token                    equals_sign_token;
                utl::Wrapper<Expression> expression;
            };

            Qualified_name                        constructor;
            Surrounded<Separated_sequence<Field>> initializers;
        };

        struct Binary_operator_chain {
            struct Operator_name {
                kieli::Identifier identifier;
                utl::Source_range source_range;
            };

            struct Operator_and_operand {
                utl::Wrapper<Expression> right_operand;
                Operator_name            operator_name;
            };

            std::vector<Operator_and_operand> sequence_tail;
            utl::Wrapper<Expression>          leftmost_operand;
        };

        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            kieli::Name_lower        field_name;
            Token                    dot_token;
        };

        struct Tuple_field_access {
            utl::Wrapper<Expression> base_expression;
            std::uint64_t            field_index {};
            Token                    field_index_token;
            Token                    dot_token;
        };

        struct Array_index_access {
            utl::Wrapper<Expression>             base_expression;
            Surrounded<utl::Wrapper<Expression>> index_expression;
            Token                                dot_token;
        };

        struct Method_invocation {
            Function_arguments                function_arguments;
            std::optional<Template_arguments> template_arguments;
            utl::Wrapper<Expression>          base_expression;
            kieli::Name_lower                 method_name;
        };

        struct Conditional {
            struct False_branch {
                utl::Wrapper<Expression> body;
                Token                    else_or_elif_keyword_token;
            };

            utl::Wrapper<Expression>    condition;
            utl::Wrapper<Expression>    true_branch;
            std::optional<False_branch> false_branch;
            Token                       if_or_elif_keyword_token;
            utl::Explicit<bool>         is_elif;
        };

        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
                Token                    arrow_token;
                std::optional<Token>     optional_semicolon_token;
            };

            Surrounded<std::vector<Case>> cases;
            utl::Wrapper<Expression>      matched_expression;
            Token                         match_keyword_token;
        };

        struct Type_cast {
            utl::Wrapper<Expression> base_expression;
            Token                    as_token;
            utl::Wrapper<Type>       target_type;
        };

        struct Type_ascription {
            utl::Wrapper<Expression> base_expression;
            Token                    colon_token;
            utl::Wrapper<Type>       ascribed_type;
        };

        struct Let_binding {
            utl::Wrapper<Pattern>          pattern;
            std::optional<Type_annotation> type;
            utl::Wrapper<Expression>       initializer;
            Token                          let_keyword_token;
            Token                          equals_sign_token;
        };

        struct Conditional_let {
            utl::Wrapper<Pattern>    pattern;
            utl::Wrapper<Expression> initializer;
            Token                    let_keyword_token;
            Token                    equals_sign_token;
        };

        struct Local_type_alias {
            kieli::Name_upper  alias_name;
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

        struct Continue {
            Token continue_keyword_token;
        };

        struct Break {
            std::optional<utl::Wrapper<Expression>> result;
            Token                                   break_keyword_token;
        };

        struct Discard {
            utl::Wrapper<Expression> discarded_expression;
            Token                    discard_keyword_token;
        };

        struct Ret {
            std::optional<utl::Wrapper<Expression>> returned_expression;
            Token                                   ret_keyword_token;
        };

        struct Sizeof {
            Surrounded<utl::Wrapper<Type>> inspected_type;
            Token                          sizeof_keyword_token;
        };

        struct Addressof {
            std::optional<Mutability> mutability;
            utl::Wrapper<Expression>  lvalue_expression;
            Token                     ampersand_token;
        };

        struct Dereference {
            utl::Wrapper<Expression> reference_expression;
            Token                    asterisk_token;
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
    } // namespace expression

    struct Expression {
        using Variant = std::variant<
            kieli::Integer,
            kieli::Floating,
            kieli::Character,
            kieli::Boolean,
            kieli::String,
            expression::Parenthesized,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Template_application,
            expression::Tuple,
            expression::Block,
            expression::Invocation,
            expression::Unit_initializer,
            expression::Tuple_initializer,
            expression::Struct_initializer,
            expression::Binary_operator_chain,
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
            expression::Addressof,
            expression::Dereference,
            expression::Unsafe,
            expression::Move,
            expression::Meta,
            expression::Hole>;

        Variant           variant;
        utl::Source_range source_range;
    };

    namespace pattern {
        struct Parenthesized {
            Surrounded<utl::Wrapper<Pattern>> pattern;
        };

        struct Name {
            kieli::Name_lower         name;
            std::optional<Mutability> mutability;
        };

        struct Field {
            struct Field_pattern {
                Token                 equals_sign_token;
                utl::Wrapper<Pattern> pattern;
            };

            kieli::Name_lower            name;
            std::optional<Field_pattern> field_pattern;
        };

        struct Struct_constructor {
            Surrounded<Separated_sequence<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<utl::Wrapper<Pattern>> pattern;
        };

        struct Unit_constructor {};

        using Constructor_body = std::variant<
            Struct_constructor, //
            Tuple_constructor,
            Unit_constructor>;

        struct Constructor {
            Qualified_name   name;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            kieli::Name_upper name;
            Constructor_body  body;
            Token             double_colon_token;
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
            std::optional<Mutability> alias_mutability;
            kieli::Name_lower         alias_name;
            utl::Wrapper<Pattern>     aliased_pattern;
            Token                     as_keyword_token;
        };

        struct Guarded {
            utl::Wrapper<Pattern>    guarded_pattern;
            utl::Wrapper<Expression> guard_expression;
            Token                    if_keyword_token;
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
            pattern::Parenthesized,
            pattern::Name,
            pattern::Constructor,
            pattern::Abbreviated_constructor,
            pattern::Tuple,
            pattern::Top_level_tuple,
            pattern::Slice,
            pattern::Alias,
            pattern::Guarded>;

        Variant           variant;
        utl::Source_range source_range;
    };

    namespace type {
        struct Parenthesized {
            Surrounded<utl::Wrapper<Type>> type;
        };

        struct Self {};

        struct Typename {
            Qualified_name name;
        };

        struct Tuple {
            Surrounded<Separated_sequence<utl::Wrapper<Type>>> field_types;
        };

        struct Array {
            utl::Wrapper<Type>       element_type;
            utl::Wrapper<Expression> length;
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
            std::optional<Mutability> mutability;
            utl::Wrapper<Type>        referenced_type;
            Token                     ampersand_token;
        };

        struct Pointer {
            std::optional<Mutability> mutability;
            utl::Wrapper<Type>        pointee_type;
            Token                     asterisk_token;
        };

        struct Instance_of {
            Separated_sequence<Class_reference> classes;
            Token                               inst_keyword_token;
        };

        struct Template_application {
            Template_arguments template_arguments;
            Qualified_name     name;
        };
    } // namespace type

    struct Type {
        using Variant = std::variant<
            type::Parenthesized,
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
            type::Instance_of,
            type::Reference,
            type::Pointer,
            type::Template_application>;

        Variant           variant;
        utl::Source_range source_range;
    };

    struct Function_signature {
        std::optional<Template_parameters> template_parameters;
        Surrounded<Function_parameters>    function_parameters;
        std::optional<Type_annotation>     return_type;
        kieli::Name_lower                  name;
        Token                              fn_keyword_token;
    };

    struct Type_signature {
        std::optional<Template_parameters>  template_parameters;
        Separated_sequence<Class_reference> classes;
        kieli::Name_upper                   name;
        std::optional<Token>                classes_colon_token;
        Token                               alias_keyword_token;
    };

    namespace definition {
        struct Function {
            Function_signature       signature;
            utl::Wrapper<Expression> body;
            std::optional<Token>     optional_equals_sign_token;
            Token                    fn_keyword_token;
        };

        struct Field {
            kieli::Name_lower name;
            Type_annotation   type;
            utl::Source_range source_range;
        };

        struct Struct_constructor {
            Surrounded<Separated_sequence<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<Separated_sequence<utl::Wrapper<Type>>> types;
        };

        struct Unit_constructor {};

        using Constructor_body
            = std::variant<Struct_constructor, Tuple_constructor, Unit_constructor>;

        struct Constructor {
            kieli::Name_upper name;
            Constructor_body  body;
        };

        struct Struct {
            std::optional<Template_parameters> template_parameters;
            Constructor_body                   body;
            kieli::Name_upper                  name;
            Token                              struct_keyword_token;
        };

        struct Enum {
            std::optional<Template_parameters> template_parameters;
            Separated_sequence<Constructor>    constructors;
            kieli::Name_upper                  name;
            Token                              enum_keyword_token;
            Token                              equals_sign_token;
        };

        struct Alias {
            std::optional<Template_parameters> template_parameters;
            kieli::Name_upper                  name;
            utl::Wrapper<Type>                 type;
            Token                              alias_keyword_token;
            Token                              equals_sign_token;
        };

        struct Typeclass {
            std::optional<Template_parameters> template_parameters;
            std::vector<Function_signature>    function_signatures;
            std::vector<Type_signature>        type_signatures;
            kieli::Name_upper                  name;
            Token                              class_keyword_token;
            Token                              open_brace_token;
            Token                              close_brace_token;
        };

        struct Implementation {
            std::optional<Template_parameters>  template_parameters;
            Surrounded<std::vector<Definition>> definitions;
            utl::Wrapper<Type>                  self_type;
            Token                               impl_keyword_token;
        };

        struct Instantiation {
            std::optional<Template_parameters>  template_parameters;
            Class_reference                     typeclass;
            Surrounded<std::vector<Definition>> definitions;
            utl::Wrapper<Type>                  self_type;
            Token                               inst_keyword_token;
            Token                               for_keyword_token;
        };

        struct Submodule {
            std::optional<Template_parameters>  template_parameters;
            Surrounded<std::vector<Definition>> definitions;
            kieli::Name_lower                   name;
            Token                               module_keyword_token;
        };
    } // namespace definition

    struct Definition {
        using Variant = std::variant<
            definition::Function,
            definition::Struct,
            definition::Enum,
            definition::Alias,
            definition::Typeclass,
            definition::Implementation,
            definition::Instantiation,
            definition::Submodule>;

        Variant              variant;
        utl::Source::Wrapper source;
        utl::Source_range    source_range;
    };

    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct [[nodiscard]] Module {
        struct Import {
            Separated_sequence<kieli::Name_lower> segments;
            Token                                 import_keyword_token;
        };

        std::vector<Import>     imports;
        std::vector<Definition> definitions;
        Node_arena              node_arena;
        utl::Source::Wrapper    source;
    };

} // namespace cst
