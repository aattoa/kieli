#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>
#include <libcompiler/token/token.hpp>

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

namespace kieli::cst {
    struct Expression_id : utl::Vector_index<Expression_id> {
        using Vector_index::Vector_index;
    };

    struct Pattern_id : utl::Vector_index<Pattern_id> {
        using Vector_index::Vector_index;
    };

    struct Type_id : utl::Vector_index<Type_id> {
        using Vector_index::Vector_index;
    };

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Definition;

    struct [[nodiscard]] Token {
        kieli::Range     range;
        std::string_view preceding_trivia;

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
        kieli::Lower name;
        Token        equals_sign_token;
    };

    struct Type_annotation {
        Type_id type;
        Token   colon_token;
    };

    struct Wildcard {
        kieli::Range range;
    };

    namespace mutability {
        using Concrete = kieli::Mutability;

        struct Parameterized {
            kieli::Lower name;
            Token        question_mark_token;
        };
    } // namespace mutability

    struct Mutability_variant : std::variant<mutability::Concrete, mutability::Parameterized> {
        using variant::variant, variant::operator=;
    };

    struct Mutability {
        Mutability_variant variant;
        kieli::Range       range;
        Token              mut_or_immut_keyword_token;
    };

    struct Self_parameter {
        std::optional<Mutability> mutability;
        std::optional<Token>      ampersand_token;
        Token                     self_keyword_token;

        [[nodiscard]] auto is_reference() const noexcept -> bool;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant, variant::operator=;
    };

    using Template_arguments = Surrounded<Separated_sequence<Template_argument>>;

    struct Path_segment {
        std::optional<Template_arguments> template_arguments;
        kieli::Name                       name;
        std::optional<Token>              trailing_double_colon_token;
    };

    struct Path_root_global {
        Token global_keyword;
    };

    struct Path_root {
        using Variant = std::variant<Path_root_global, Type_id>;
        Variant      variant;
        Token        double_colon_token;
        kieli::Range range;
    };

    struct Path {
        Separated_sequence<Path_segment> segments;
        std::optional<Path_root>         root;
        kieli::Name                      head;
        kieli::Range                     range;

        [[nodiscard]] auto is_simple_name() const noexcept -> bool;
    };

    struct Concept_reference {
        std::optional<Template_arguments> template_arguments;
        Path                              path;
        kieli::Range                      range;
    };

    template <class T>
    struct Default_argument {
        std::variant<T, Wildcard> variant;
        Token                     equals_sign_token;
    };

    using Type_parameter_default_argument       = Default_argument<Type_id>;
    using Value_parameter_default_argument      = Default_argument<Expression_id>;
    using Mutability_parameter_default_argument = Default_argument<Mutability>;

    struct Function_parameter {
        Pattern_id                                      pattern;
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
        Expression_id                    expression;
    };

    using Function_arguments = Surrounded<Separated_sequence<Function_argument>>;

    struct Template_type_parameter {
        kieli::Upper                                   name;
        std::optional<Token>                           colon_token;
        Separated_sequence<Concept_reference>          concepts;
        std::optional<Type_parameter_default_argument> default_argument;
    };

    struct Template_value_parameter {
        kieli::Lower                                    name;
        std::optional<Type_annotation>                  type_annotation;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    struct Template_mutability_parameter {
        kieli::Lower                                         name;
        Token                                                colon_token;
        Token                                                mut_keyword_token;
        std::optional<Mutability_parameter_default_argument> default_argument;
    };

    struct Template_parameter_variant
        : std::variant<
              Template_type_parameter,
              Template_value_parameter,
              Template_mutability_parameter> {
        using variant::variant, variant::operator=;
    };

    struct Template_parameter {
        Template_parameter_variant variant;
        kieli::Range               range;
    };

    using Template_parameters = Surrounded<Separated_sequence<Template_parameter>>;

    namespace expression {
        struct Parenthesized {
            Surrounded<Expression_id> expression;
        };

        struct Array_literal {
            Surrounded<Separated_sequence<Expression_id>> elements;
        };

        struct Self {};

        struct Variable {
            Path path;
        };

        struct Template_application {
            Template_arguments template_arguments;
            Path               path;
        };

        struct Tuple {
            Surrounded<Separated_sequence<Expression_id>> fields;
        };

        struct Block {
            struct Side_effect {
                Expression_id expression;
                Token         trailing_semicolon_token;
            };

            std::vector<Side_effect>     side_effects;
            std::optional<Expression_id> result_expression;
            Token                        open_brace_token;
            Token                        close_brace_token;
        };

        struct Invocation {
            Function_arguments function_arguments;
            Expression_id      function_expression;
        };

        struct Unit_initializer {
            Path constructor_path;
        };

        struct Tuple_initializer {
            Path                                          constructor_path;
            Surrounded<Separated_sequence<Expression_id>> initializers;
        };

        struct Struct_initializer {
            struct Field {
                kieli::Lower  name;
                Token         equals_sign_token;
                Expression_id expression;
            };

            Path                                  constructor_path;
            Surrounded<Separated_sequence<Field>> initializers;
        };

        struct Operator_name {
            kieli::Identifier identifier;
            kieli::Range      range;
        };

        struct Operator_chain {
            struct Rhs {
                Expression_id operand;
                Operator_name operator_name;
            };

            std::vector<Rhs> tail;
            Expression_id    lhs;
        };

        struct Struct_field_access {
            Expression_id base_expression;
            kieli::Lower  field_name;
            Token         dot_token;
        };

        struct Tuple_field_access {
            Expression_id base_expression;
            std::uint64_t field_index {};
            Token         field_index_token;
            Token         dot_token;
        };

        struct Array_index_access {
            Expression_id             base_expression;
            Surrounded<Expression_id> index_expression;
            Token                     dot_token;
        };

        struct Method_invocation {
            Function_arguments                function_arguments;
            std::optional<Template_arguments> template_arguments;
            Expression_id                     base_expression;
            kieli::Lower                      method_name;
        };

        struct Conditional {
            struct False_branch {
                Expression_id body;
                Token         else_or_elif_keyword_token;
            };

            Expression_id               condition;
            Expression_id               true_branch;
            std::optional<False_branch> false_branch;
            Token                       if_or_elif_keyword_token;
            utl::Explicit<bool>         is_elif;
        };

        struct Match {
            struct Case {
                Pattern_id           pattern;
                Expression_id        handler;
                Token                arrow_token;
                std::optional<Token> optional_semicolon_token;
            };

            Surrounded<std::vector<Case>> cases;
            Expression_id                 matched_expression;
            Token                         match_keyword_token;
        };

        struct Type_cast {
            Expression_id base_expression;
            Token         as_token;
            Type_id       target_type;
        };

        struct Type_ascription {
            Expression_id base_expression;
            Token         colon_token;
            Type_id       ascribed_type;
        };

        struct Let_binding {
            Pattern_id                     pattern;
            std::optional<Type_annotation> type;
            Expression_id                  initializer;
            Token                          let_keyword_token;
            Token                          equals_sign_token;
        };

        struct Conditional_let {
            Pattern_id    pattern;
            Expression_id initializer;
            Token         let_keyword_token;
            Token         equals_sign_token;
        };

        struct Local_type_alias {
            kieli::Upper name;
            Type_id      type;
            Token        alias_keyword_token;
            Token        equals_sign_token;
        };

        struct Infinite_loop {
            Expression_id body;
            Token         loop_keyword_token;
        };

        struct While_loop {
            Expression_id condition;
            Expression_id body;
            Token         while_keyword_token;
        };

        struct For_loop {
            Pattern_id    iterator;
            Expression_id iterable;
            Expression_id body;
            Token         for_keyword_token;
            Token         in_keyword_token;
        };

        struct Continue {
            Token continue_keyword_token;
        };

        struct Break {
            std::optional<Expression_id> result;
            Token                        break_keyword_token;
        };

        struct Discard {
            Expression_id discarded_expression;
            Token         discard_keyword_token;
        };

        struct Ret {
            std::optional<Expression_id> returned_expression;
            Token                        ret_keyword_token;
        };

        struct Sizeof {
            Surrounded<Type_id> inspected_type;
            Token               sizeof_keyword_token;
        };

        struct Addressof {
            std::optional<Mutability> mutability;
            Expression_id             place_expression;
            Token                     ampersand_token;
        };

        struct Dereference {
            Expression_id reference_expression;
            Token         asterisk_token;
        };

        struct Unsafe {
            Expression_id expression;
            Token         unsafe_keyword_token;
        };

        struct Move {
            Expression_id place_expression;
            Token         mov_keyword_token;
        };

        struct Defer {
            Expression_id expression;
            Token         defer_keyword_token;
        };

        struct Meta {
            Surrounded<Expression_id> expression;
            Token                     meta_keyword_token;
        };

        struct Hole {};
    } // namespace expression

    struct Expression_variant
        : std::variant<
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
              expression::Operator_chain,
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
              expression::Defer,
              expression::Meta,
              expression::Hole> {
        using variant::variant, variant::operator=;
    };

    struct Expression {
        Expression_variant variant;
        kieli::Range       range;
    };

    namespace pattern {
        struct Parenthesized {
            Surrounded<Pattern_id> pattern;
        };

        struct Name {
            kieli::Lower              name;
            std::optional<Mutability> mutability;
        };

        struct Field {
            struct Field_pattern {
                Token      equals_sign_token;
                Pattern_id pattern;
            };

            kieli::Lower                 name;
            std::optional<Field_pattern> field_pattern;
        };

        struct Struct_constructor {
            Surrounded<Separated_sequence<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<Pattern_id> pattern;
        };

        struct Unit_constructor {};

        struct Constructor_body
            : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
            using variant::variant, variant::operator=;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            kieli::Upper     name;
            Constructor_body body;
            Token            double_colon_token;
        };

        struct Tuple {
            Surrounded<Separated_sequence<Pattern_id>> patterns;
        };

        struct Top_level_tuple {
            Separated_sequence<Pattern_id> patterns;
        };

        struct Slice {
            Surrounded<Separated_sequence<Pattern_id>> patterns;
        };

        struct Alias {
            std::optional<Mutability> mutability;
            kieli::Lower              name;
            Pattern_id                pattern;
            Token                     as_keyword_token;
        };

        struct Guarded {
            Pattern_id    guarded_pattern;
            Expression_id guard_expression;
            Token         if_keyword_token;
        };
    } // namespace pattern

    struct Pattern_variant
        : std::variant<
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
              pattern::Guarded> {
        using variant::variant, variant::operator=;
    };

    struct Pattern {
        Pattern_variant variant;
        kieli::Range    range;
    };

    namespace type {
        struct Parenthesized {
            Surrounded<Type_id> type;
        };

        struct Self {};

        struct Typename {
            Path path;
        };

        struct Tuple {
            Surrounded<Separated_sequence<Type_id>> field_types;
        };

        struct Array {
            Type_id       element_type;
            Expression_id length;
            Token         open_bracket_token;
            Token         close_bracket_token;
            Token         semicolon_token;
        };

        struct Slice {
            Surrounded<Type_id> element_type;
        };

        struct Function {
            Surrounded<Separated_sequence<Type_id>> parameter_types;
            Type_annotation                         return_type;
            Token                                   fn_keyword_token;
        };

        struct Typeof {
            Surrounded<Expression_id> inspected_expression;
            Token                     typeof_keyword_token;
        };

        struct Reference {
            std::optional<Mutability> mutability;
            Type_id                   referenced_type;
            Token                     ampersand_token;
        };

        struct Pointer {
            std::optional<Mutability> mutability;
            Type_id                   pointee_type;
            Token                     asterisk_token;
        };

        struct Implementation {
            Separated_sequence<Concept_reference> concepts;
            Token                                 impl_keyword_token;
        };

        struct Template_application {
            Template_arguments template_arguments;
            Path               path;
        };
    } // namespace type

    struct Type_variant
        : std::variant<
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
              type::Implementation,
              type::Reference,
              type::Pointer,
              type::Template_application> {
        using variant::variant, variant::operator=;
    };

    struct Type {
        Type_variant variant;
        kieli::Range range;
    };

    struct Function_signature {
        std::optional<Template_parameters> template_parameters;
        Surrounded<Function_parameters>    function_parameters;
        std::optional<Type_annotation>     return_type;
        kieli::Lower                       name;
        Token                              fn_keyword_token;
    };

    struct Type_signature {
        std::optional<Template_parameters>    template_parameters;
        Separated_sequence<Concept_reference> concepts;
        kieli::Upper                          name;
        std::optional<Token>                  concepts_colon_token;
        Token                                 alias_keyword_token;
    };

    using Concept_requirement = std::variant<Function_signature, Type_signature>;

    namespace definition {
        struct Function {
            Function_signature   signature;
            Expression_id        body;
            std::optional<Token> optional_equals_sign_token;
            Token                fn_keyword_token;
        };

        struct Field {
            kieli::Lower    name;
            Type_annotation type;
            kieli::Range    range;
        };

        struct Struct_constructor {
            Surrounded<Separated_sequence<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<Separated_sequence<Type_id>> types;
        };

        struct Unit_constructor {};

        struct Constructor_body
            : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
            using variant::variant, variant::operator=;
        };

        struct Constructor {
            kieli::Upper     name;
            Constructor_body body;
        };

        struct Struct {
            std::optional<Template_parameters> template_parameters;
            Constructor_body                   body;
            kieli::Upper                       name;
            Token                              struct_keyword_token;
        };

        struct Enum {
            std::optional<Template_parameters> template_parameters;
            Separated_sequence<Constructor>    constructors;
            kieli::Upper                       name;
            Token                              enum_keyword_token;
            Token                              equals_sign_token;
        };

        struct Alias {
            std::optional<Template_parameters> template_parameters;
            kieli::Upper                       name;
            Type_id                            type;
            Token                              alias_keyword_token;
            Token                              equals_sign_token;
        };

        struct Concept {
            std::optional<Template_parameters> template_parameters;
            std::vector<Concept_requirement>   requirements;
            kieli::Upper                       name;
            Token                              concept_keyword_token;
            Token                              open_brace_token;
            Token                              close_brace_token;
        };

        struct Implementation {
            std::optional<Template_parameters>  template_parameters;
            Surrounded<std::vector<Definition>> definitions;
            Type_id                             self_type;
            Token                               impl_keyword_token;
        };

        struct Submodule {
            std::optional<Template_parameters>  template_parameters;
            Surrounded<std::vector<Definition>> definitions;
            kieli::Lower                        name;
            Token                               module_keyword_token;
        };
    } // namespace definition

    struct Definition_variant
        : std::variant<
              definition::Function,
              definition::Struct,
              definition::Enum,
              definition::Alias,
              definition::Concept,
              definition::Implementation,
              definition::Submodule> {
        using variant::variant, variant::operator=;
    };

    struct Definition {
        Definition_variant variant;
        kieli::Source_id   source;
        kieli::Range       range;
    };

    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    struct Arena {
        utl::Index_vector<Expression_id, Expression> expressions;
        utl::Index_vector<Pattern_id, Pattern>       patterns;
        utl::Index_vector<Type_id, Type>             types;
    };

    struct Import {
        Separated_sequence<kieli::Lower> segments;
        Token                            import_keyword_token;
    };
} // namespace kieli::cst

struct kieli::CST::Module {
    std::vector<cst::Import>     imports;
    std::vector<cst::Definition> definitions;
    cst::Arena                   arena;
    kieli::Source_id             source;
};
