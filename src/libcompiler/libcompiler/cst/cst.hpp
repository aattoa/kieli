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
    // Since tokens are used all over the CST, this saves a significant amount of memory.
    struct Token_id : utl::Vector_index<Token_id, std::uint32_t> {
        using Vector_index::Vector_index;
    };

    struct Token {
        Range            range;
        std::string_view preceding_trivia;
    };

    template <typename T>
    struct Surrounded {
        T        value;
        Token_id open_token;
        Token_id close_token;
    };

    template <typename T>
    struct Separated {
        std::vector<T>        elements;
        std::vector<Token_id> separator_tokens;
    };

    struct Type_annotation {
        Type_id  type;
        Token_id colon_token;
    };

    struct Wildcard {
        Token_id underscore_token;
    };

    struct Parameterized_mutability {
        Lower    name;
        Token_id question_mark_token;
    };

    struct Mutability_variant : std::variant<kieli::Mutability, Parameterized_mutability> {
        using variant::variant;
    };

    struct Mutability {
        Mutability_variant variant;
        Range              range;
        Token_id           mut_or_immut_token;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant;
    };

    using Template_arguments = Surrounded<Separated<Template_argument>>;

    struct Path_segment {
        std::optional<Template_arguments> template_arguments;
        Name                              name;
        std::optional<Token_id>           leading_double_colon_token;
    };

    struct Path_root_global {
        Token_id global_token;
    };

    using Path_root = std::variant<std::monostate, Path_root_global, Type_id>;

    struct Path {
        Path_root                 root;
        std::vector<Path_segment> segments;
        Range                     range;

        [[nodiscard]] auto head() const -> Path_segment const&;
        [[nodiscard]] auto is_upper() const -> bool;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    template <typename T>
    struct Default_argument {
        std::variant<T, Wildcard> variant;
        Token_id                  equals_sign_token;
    };

    using Type_parameter_default_argument       = Default_argument<Type_id>;
    using Value_parameter_default_argument      = Default_argument<Expression_id>;
    using Mutability_parameter_default_argument = Default_argument<Mutability>;

    struct Function_parameter {
        Pattern_id                                      pattern;
        std::optional<Type_annotation>                  type;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    using Function_parameters = Surrounded<Separated<Function_parameter>>;
    using Function_arguments  = Surrounded<Separated<Expression_id>>;

    struct Template_type_parameter {
        Upper                                          name;
        std::optional<Token_id>                        colon_token;
        Separated<Path>                                concepts;
        std::optional<Type_parameter_default_argument> default_argument;
    };

    struct Template_value_parameter {
        Lower                                           name;
        Type_annotation                                 type_annotation;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    struct Template_mutability_parameter {
        Lower                                                name;
        Token_id                                             colon_token;
        Token_id                                             mut_token;
        std::optional<Mutability_parameter_default_argument> default_argument;
    };

    struct Template_parameter_variant
        : std::variant<
              Template_type_parameter,
              Template_value_parameter,
              Template_mutability_parameter> {
        using variant::variant;
    };

    struct Template_parameter {
        Template_parameter_variant variant;
        Range                      range;
    };

    using Template_parameters = Surrounded<Separated<Template_parameter>>;

    struct Struct_field_equals {
        Token_id      equals_sign_token;
        Expression_id expression;
    };

    struct Struct_field_initializer {
        Lower                              name;
        std::optional<Struct_field_equals> equals;
    };

    struct Match_case {
        Pattern_id              pattern;
        Expression_id           handler;
        Token_id                arrow_token;
        std::optional<Token_id> optional_semicolon_token;
    };

    namespace expression {
        struct Paren {
            Surrounded<Expression_id> expression;
        };

        struct Array {
            Surrounded<Separated<Expression_id>> elements;
        };

        struct Tuple {
            Surrounded<Separated<Expression_id>> fields;
        };

        struct Block {
            struct Side_effect {
                Expression_id expression;
                Token_id      trailing_semicolon_token;
            };

            std::vector<Side_effect>     side_effects;
            std::optional<Expression_id> result_expression;
            Token_id                     open_brace_token;
            Token_id                     close_brace_token;
        };

        struct Function_call {
            Function_arguments arguments;
            Expression_id      invocable;
        };

        struct Tuple_initializer {
            Path                                 constructor_path;
            Surrounded<Separated<Expression_id>> initializers;
        };

        struct Struct_initializer {
            Path                                            constructor_path;
            Surrounded<Separated<Struct_field_initializer>> initializers;
        };

        struct Infix_name {
            Identifier identifier;
            Range      range;
        };

        struct Infix_chain {
            struct Rhs {
                Expression_id operand;
                Infix_name    infix_name;
            };

            std::vector<Rhs> tail;
            Expression_id    lhs;
        };

        struct Struct_field {
            Expression_id base_expression;
            Lower         name;
            Token_id      dot_token;
        };

        struct Tuple_field {
            Expression_id base_expression;
            std::uint64_t field_index {};
            Token_id      field_index_token;
            Token_id      dot_token;
        };

        struct Array_index {
            Expression_id             base_expression;
            Surrounded<Expression_id> index_expression;
            Token_id                  dot_token;
        };

        struct Method_call {
            Function_arguments                function_arguments;
            std::optional<Template_arguments> template_arguments;
            Expression_id                     base_expression;
            Lower                             method_name;
        };

        struct False_branch {
            Expression_id body;
            Token_id      else_or_elif_token;
        };

        struct Conditional {
            Expression_id               condition;
            Expression_id               true_branch;
            std::optional<False_branch> false_branch;
            Token_id                    if_or_elif_token;
            utl::Explicit<bool>         is_elif;
        };

        struct Match {
            Surrounded<std::vector<Match_case>> cases;
            Expression_id                       matched_expression;
            Token_id                            match_token;
        };

        struct Ascription {
            Expression_id base_expression;
            Token_id      colon_token;
            Type_id       ascribed_type;
        };

        struct Let {
            Pattern_id                     pattern;
            std::optional<Type_annotation> type;
            Expression_id                  initializer;
            Token_id                       let_token;
            Token_id                       equals_sign_token;
        };

        struct Conditional_let {
            Pattern_id    pattern;
            Expression_id initializer;
            Token_id      let_token;
            Token_id      equals_sign_token;
        };

        struct Type_alias {
            Upper    name;
            Type_id  type;
            Token_id alias_token;
            Token_id equals_sign_token;
        };

        struct Loop {
            Expression_id body;
            Token_id      loop_token;
        };

        struct While_loop {
            Expression_id condition;
            Expression_id body;
            Token_id      while_token;
        };

        struct For_loop {
            Pattern_id    iterator;
            Expression_id iterable;
            Expression_id body;
            Token_id      for_token;
            Token_id      in_token;
        };

        struct Continue {
            Token_id continue_token;
        };

        struct Break {
            std::optional<Expression_id> result;
            Token_id                     break_token;
        };

        struct Ret {
            std::optional<Expression_id> returned_expression;
            Token_id                     ret_token;
        };

        struct Sizeof {
            Surrounded<Type_id> inspected_type;
            Token_id            sizeof_token;
        };

        struct Addressof {
            std::optional<Mutability> mutability;
            Expression_id             place_expression;
            Token_id                  ampersand_token;
        };

        struct Dereference {
            Expression_id reference_expression;
            Token_id      asterisk_token;
        };

        struct Move {
            Expression_id place_expression;
            Token_id      mov_token;
        };

        struct Defer {
            Expression_id effect_expression;
            Token_id      defer_token;
        };
    } // namespace expression

    struct Expression_variant
        : std::variant<
              Integer,
              Floating,
              Character,
              Boolean,
              String,
              Wildcard,
              Path,
              expression::Paren,
              expression::Array,
              expression::Tuple,
              expression::Block,
              expression::Function_call,
              expression::Tuple_initializer,
              expression::Struct_initializer,
              expression::Infix_chain,
              expression::Struct_field,
              expression::Tuple_field,
              expression::Array_index,
              expression::Method_call,
              expression::Conditional,
              expression::Match,
              expression::Ascription,
              expression::Let,
              expression::Conditional_let,
              expression::Type_alias,
              expression::Loop,
              expression::While_loop,
              expression::For_loop,
              expression::Continue,
              expression::Break,
              expression::Ret,
              expression::Sizeof,
              expression::Addressof,
              expression::Dereference,
              expression::Move,
              expression::Defer> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant variant;
        Range              range;
    };

    namespace pattern {
        struct Paren {
            Surrounded<Pattern_id> pattern;
        };

        struct Name {
            Lower                     name;
            std::optional<Mutability> mutability;
        };

        struct Equals {
            Token_id   equals_sign_token;
            Pattern_id pattern;
        };

        struct Field {
            Lower                 name;
            std::optional<Equals> equals;
        };

        struct Unit_constructor {};

        struct Struct_constructor {
            Surrounded<Separated<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<Pattern_id> pattern;
        };

        struct Constructor_body
            : std::variant<Unit_constructor, Struct_constructor, Tuple_constructor> {
            using variant::variant;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            Upper            name;
            Constructor_body body;
            Token_id         double_colon_token;
        };

        struct Tuple {
            Surrounded<Separated<Pattern_id>> patterns;
        };

        struct Top_level_tuple {
            Separated<Pattern_id> patterns;
        };

        struct Slice {
            Surrounded<Separated<Pattern_id>> patterns;
        };

        struct Guarded {
            Pattern_id    guarded_pattern;
            Expression_id guard_expression;
            Token_id      if_token;
        };
    } // namespace pattern

    struct Pattern_variant
        : std::variant<
              Integer,
              Floating,
              Character,
              Boolean,
              String,
              Wildcard,
              pattern::Paren,
              pattern::Name,
              pattern::Constructor,
              pattern::Abbreviated_constructor,
              pattern::Tuple,
              pattern::Top_level_tuple,
              pattern::Slice,
              pattern::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        Range           range;
    };

    namespace type {
        struct Paren {
            Surrounded<Type_id> type;
        };

        struct Never {
            Token_id exclamation_token;
        };

        struct Tuple {
            Surrounded<Separated<Type_id>> field_types;
        };

        struct Array {
            Type_id       element_type;
            Expression_id length;
            Token_id      open_bracket_token;
            Token_id      close_bracket_token;
            Token_id      semicolon_token;
        };

        struct Slice {
            Surrounded<Type_id> element_type;
        };

        struct Function {
            Surrounded<Separated<Type_id>> parameter_types;
            Type_annotation                return_type;
            Token_id                       fn_token;
        };

        struct Typeof {
            Surrounded<Expression_id> inspected_expression;
            Token_id                  typeof_token;
        };

        struct Reference {
            std::optional<Mutability> mutability;
            Type_id                   referenced_type;
            Token_id                  ampersand_token;
        };

        struct Pointer {
            std::optional<Mutability> mutability;
            Type_id                   pointee_type;
            Token_id                  asterisk_token;
        };

        struct Impl {
            Separated<Path> concepts;
            Token_id        impl_token;
        };
    } // namespace type

    struct Type_variant
        : std::variant<
              Wildcard,
              Path,
              type::Paren,
              type::Never,
              type::Tuple,
              type::Array,
              type::Slice,
              type::Function,
              type::Typeof,
              type::Impl,
              type::Reference,
              type::Pointer> {
        using variant::variant;
    };

    struct Type {
        Type_variant variant;
        Range        range;
    };

    struct Function_signature {
        std::optional<Template_parameters> template_parameters;
        Function_parameters                function_parameters;
        std::optional<Type_annotation>     return_type;
        Lower                              name;
        Token_id                           fn_token;
    };

    struct Type_signature {
        std::optional<Template_parameters> template_parameters;
        Separated<Path>                    concepts;
        Upper                              name;
        std::optional<Token_id>            concepts_colon_token;
        Token_id                           alias_token;
    };

    using Concept_requirement = std::variant<Function_signature, Type_signature>;

    namespace definition {
        struct Function {
            Function_signature      signature;
            Expression_id           body;
            std::optional<Token_id> optional_equals_sign_token;
            Token_id                fn_token;
        };

        struct Field {
            Lower           name;
            Type_annotation type;
            Range           range;
        };

        struct Struct_constructor {
            Surrounded<Separated<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<Separated<Type_id>> types;
        };

        struct Unit_constructor {};

        struct Constructor_body
            : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
            using variant::variant;
        };

        struct Constructor {
            Upper            name;
            Constructor_body body;
        };

        struct Struct {
            std::optional<Template_parameters> template_parameters;
            Constructor_body                   body;
            Upper                              name;
            Token_id                           struct_token;
        };

        struct Enum {
            std::optional<Template_parameters> template_parameters;
            Separated<Constructor>             constructors;
            Upper                              name;
            Token_id                           enum_token;
            Token_id                           equals_sign_token;
        };

        struct Alias {
            std::optional<Template_parameters> template_parameters;
            Upper                              name;
            Type_id                            type;
            Token_id                           alias_token;
            Token_id                           equals_sign_token;
        };

        struct Concept {
            std::optional<Template_parameters> template_parameters;
            std::vector<Concept_requirement>   requirements;
            Upper                              name;
            Token_id                           concept_token;
            Token_id                           open_brace_token;
            Token_id                           close_brace_token;
        };

        struct Impl {
            std::optional<Template_parameters>  template_parameters;
            Surrounded<std::vector<Definition>> definitions;
            Type_id                             self_type;
            Token_id                            impl_token;
        };

        struct Submodule {
            std::optional<Template_parameters>  template_parameters;
            Surrounded<std::vector<Definition>> definitions;
            Lower                               name;
            Token_id                            module_token;
        };
    } // namespace definition

    struct Definition_variant
        : std::variant<
              definition::Function,
              definition::Struct,
              definition::Enum,
              definition::Alias,
              definition::Concept,
              definition::Impl,
              definition::Submodule> {
        using variant::variant;
    };

    struct Definition {
        Definition_variant variant;
        Document_id        document_id;
        Range              range;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression> expressions;
        utl::Index_vector<Pattern_id, Pattern>       patterns;
        utl::Index_vector<Type_id, Type>             types;
        utl::Index_vector<Token_id, Token>           tokens;
    };

    struct Import {
        Separated<Lower> segments;
        Token_id         import_token;
    };
} // namespace kieli::cst

struct kieli::CST::Module {
    std::vector<cst::Import>     imports;
    std::vector<cst::Definition> definitions;
    cst::Arena                   arena;
    Document_id                  document_id;
};
