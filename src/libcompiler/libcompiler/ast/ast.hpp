#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

/*

    The Abstract Syntax Tree (AST) is a high level structured representation
    of a program's syntax, much like the CST, just without the exact source
    information. It is produced by desugaring the CST.

    For example, the following CST node:
        while a { b }

    would be desugared to the following AST node:
        loop { if a { b } else { break () } }

*/

namespace kieli::ast {
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
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Definition;

    struct Wildcard {
        Range range;
    };

    struct Parameterized_mutability {
        Lower name;
    };

    struct Mutability_variant : std::variant<kieli::Mutability, Parameterized_mutability> {
        using variant::variant;
    };

    struct Mutability {
        Mutability_variant variant;
        Range              range;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant;
    };

    struct Path_segment {
        std::optional<std::vector<Template_argument>> template_arguments;
        Name                                          name;
    };

    struct Path_root_global {};

    struct Path_root : std::variant<Path_root_global, Type_id> {
        using variant::variant;
    };

    struct Path {
        std::vector<Path_segment> segments;
        std::optional<Path_root>  root;
        Name                      head;

        [[nodiscard]] auto is_simple_name() const noexcept -> bool;
    };

    struct Concept_reference {
        std::optional<std::vector<Template_argument>> template_arguments;
        Path                                          path;
        Range                                         range;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type_id, Wildcard>;
        Upper                          name;
        std::vector<Concept_reference> concepts;
        std::optional<Default>         default_argument;
    };

    struct Template_value_parameter {
        using Default = std::variant<Expression_id, Wildcard>;
        Lower                  name;
        Type_id                type;
        std::optional<Default> default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        Lower                  name;
        std::optional<Default> default_argument;
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

    using Template_parameters = std::optional<std::vector<Template_parameter>>;

    struct Function_argument {
        Expression_id        expression;
        std::optional<Lower> name;
    };

    struct Function_parameter {
        Pattern_id                   pattern;
        std::optional<Type_id>       type;
        std::optional<Expression_id> default_argument;
    };

    struct Struct_field_initializer {
        Lower         name;
        Expression_id expression;
    };

    struct Match_case {
        Pattern_id    pattern;
        Expression_id expression;
    };

    enum class Loop_source { plain_loop, while_loop, for_loop };
    enum class Conditional_source { if_, elif, while_loop_body };

    auto describe_loop_source(Loop_source source) -> std::string_view;
    auto describe_conditional_source(Conditional_source source) -> std::string_view;

    namespace expression {
        struct Array_literal {
            std::vector<Expression> elements;
        };

        struct Self {};

        struct Variable {
            Path path;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            Expression_id              body;
            utl::Explicit<Loop_source> source;
        };

        struct Continue {};

        struct Break {
            Expression_id result;
        };

        struct Block {
            std::vector<Expression> side_effects;
            Expression_id           result;
        };

        struct Function_call {
            std::vector<Function_argument> arguments;
            Expression_id                  invocable;
        };

        struct Unit_initializer {
            Path constructor_path;
        };

        struct Tuple_initializer {
            Path                       constructor_path;
            std::vector<Expression_id> initializers;
        };

        struct Struct_initializer {
            Path                                  constructor_path;
            std::vector<Struct_field_initializer> initializers;
        };

        struct Binary_operator_application {
            Expression_id left;
            Expression_id right;
            Identifier    op;
            Range         op_range;
        };

        struct Struct_field_access {
            Expression_id base_expression;
            Lower         field_name;
        };

        struct Tuple_field_access {
            Expression_id              base_expression;
            utl::Explicit<std::size_t> field_index;
            Range                      field_index_range;
        };

        struct Array_index_access {
            Expression_id base_expression;
            Expression_id index_expression;
        };

        struct Method_call {
            std::vector<Function_argument>                function_arguments;
            std::optional<std::vector<Template_argument>> template_arguments;
            Expression_id                                 base_expression;
            Lower                                         method_name;
        };

        struct Conditional {
            Expression_id                     condition;
            Expression_id                     true_branch;
            Expression_id                     false_branch;
            utl::Explicit<Conditional_source> source;
            utl::Explicit<bool>               has_explicit_false_branch;
        };

        struct Match {
            std::vector<Match_case> cases;
            Expression_id           expression;
        };

        struct Template_application {
            std::vector<Template_argument> template_arguments;
            Path                           path;
        };

        struct Type_cast {
            Expression_id expression;
            Type_id       target_type;
        };

        struct Type_ascription {
            Expression_id expression;
            Type_id       ascribed_type;
        };

        struct Let_binding {
            Pattern_id    pattern;
            Expression_id initializer;
            Type_id       type;
        };

        struct Local_type_alias {
            Upper   name;
            Type_id type;
        };

        struct Ret {
            Expression_id returned_expression;
        };

        struct Sizeof {
            Type_id inspected_type;
        };

        struct Addressof {
            Mutability    mutability;
            Expression_id place_expression;
        };

        struct Dereference {
            Expression_id reference_expression;
        };

        struct Unsafe {
            Expression_id expression;
        };

        struct Move {
            Expression_id place_expression;
        };

        struct Defer {
            Expression_id effect_expression;
        };

        struct Meta {
            Expression_id expression;
        };

        struct Hole {};

        struct Error {};
    } // namespace expression

    struct Expression_variant
        : std::variant<
              Integer,
              Floating,
              Character,
              Boolean,
              String,
              expression::Array_literal,
              expression::Self,
              expression::Variable,
              expression::Tuple,
              expression::Loop,
              expression::Break,
              expression::Continue,
              expression::Block,
              expression::Function_call,
              expression::Unit_initializer,
              expression::Tuple_initializer,
              expression::Struct_initializer,
              expression::Binary_operator_application,
              expression::Struct_field_access,
              expression::Tuple_field_access,
              expression::Array_index_access,
              expression::Method_call,
              expression::Conditional,
              expression::Match,
              expression::Template_application,
              expression::Type_cast,
              expression::Type_ascription,
              expression::Let_binding,
              expression::Local_type_alias,
              expression::Ret,
              expression::Sizeof,
              expression::Addressof,
              expression::Dereference,
              expression::Unsafe,
              expression::Move,
              expression::Defer,
              expression::Meta,
              expression::Hole,
              expression::Error> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant variant;
        Range              range;
    };

    namespace pattern {
        struct Name {
            Lower      name;
            Mutability mutability;
        };

        struct Field {
            Lower                     name;
            std::optional<Pattern_id> pattern;
        };

        struct Struct_constructor {
            std::vector<Field> fields;
        };

        struct Tuple_constructor {
            Pattern_id pattern;
        };

        struct Unit_constructor {};

        struct Constructor_body
            : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
            using variant::variant;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            Upper            name;
            Constructor_body body;
        };

        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> element_patterns;
        };

        struct Alias {
            Lower      name;
            Mutability mutability;
            Pattern_id pattern;
        };

        struct Guarded {
            Pattern_id guarded_pattern;
            Expression guard_expression;
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
              pattern::Name,
              pattern::Constructor,
              pattern::Abbreviated_constructor,
              pattern::Tuple,
              pattern::Slice,
              pattern::Alias,
              pattern::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        Range           range;
    };

    namespace type {
        struct Self {};

        struct Never {};

        struct Typename {
            Path path;
        };

        struct Tuple {
            std::vector<Type> field_types;
        };

        struct Array {
            Type_id       element_type;
            Expression_id length;
        };

        struct Slice {
            Type_id element_type;
        };

        struct Function {
            std::vector<Type> parameter_types;
            Type_id           return_type;
        };

        struct Typeof {
            Expression_id inspected_expression;
        };

        struct Reference {
            Type_id    referenced_type;
            Mutability mutability;
        };

        struct Pointer {
            Type_id    pointee_type;
            Mutability mutability;
        };

        struct Implementation {
            std::vector<Concept_reference> concepts;
        };

        struct Template_application {
            std::vector<Template_argument> arguments;
            Path                           path;
        };
    } // namespace type

    struct Type_variant
        : std::variant<
              kieli::type::Integer,
              kieli::type::Floating,
              kieli::type::Character,
              kieli::type::Boolean,
              kieli::type::String,
              Wildcard,
              type::Self,
              type::Never,
              type::Typename,
              type::Tuple,
              type::Array,
              type::Slice,
              type::Function,
              type::Typeof,
              type::Reference,
              type::Pointer,
              type::Implementation,
              type::Template_application> {
        using variant::variant;
    };

    struct Type {
        Type_variant variant;
        Range        range;
    };

    struct Self_parameter {
        Mutability          mutability;
        utl::Explicit<bool> is_reference;
        Range               range;
    };

    struct Function_signature {
        Template_parameters             template_parameters;
        std::vector<Function_parameter> function_parameters;
        std::optional<Self_parameter>   self_parameter;
        Type                            return_type;
        Lower                           name;
    };

    struct Type_signature {
        std::vector<Concept_reference> concepts;
        Upper                          name;
    };

    namespace definition {
        struct Function {
            Function_signature signature;
            Expression         body;
        };

        struct Field {
            Lower name;
            Type  type;
            Range range;
        };

        struct Struct_constructor {
            std::vector<Field> fields;
        };

        struct Tuple_constructor {
            std::vector<Type_id> types;
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

        struct Enumeration {
            std::vector<Constructor> constructors;
            Upper                    name;
            Template_parameters      template_parameters;
        };

        struct Alias {
            Upper               name;
            Type                type;
            Template_parameters template_parameters;
        };

        struct Concept {
            std::vector<Function_signature> function_signatures;
            std::vector<Type_signature>     type_signatures;
            Upper                           name;
            Template_parameters             template_parameters;
        };

        struct Implementation {
            Type                    type;
            std::vector<Definition> definitions;
            Template_parameters     template_parameters;
        };

        struct Submodule {
            std::vector<Definition> definitions;
            Lower                   name;
            Template_parameters     template_parameters;
        };
    } // namespace definition

    struct Definition_variant
        : std::variant<
              definition::Function,
              definition::Enumeration,
              definition::Alias,
              definition::Concept,
              definition::Implementation,
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
    };
} // namespace kieli::ast
