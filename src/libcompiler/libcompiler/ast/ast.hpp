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
        kieli::Range range;
    };

    namespace mutability {
        using Concrete = kieli::Mutability;

        struct Parameterized {
            kieli::Lower name;
        };
    } // namespace mutability

    struct Mutability_variant : std::variant<mutability::Concrete, mutability::Parameterized> {
        using variant::variant, variant::operator=;
    };

    struct Mutability {
        Mutability_variant variant;
        kieli::Range       range;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant, variant::operator=;
    };

    struct Path_segment {
        std::optional<std::vector<Template_argument>> template_arguments;
        kieli::Name                                   name;
    };

    struct Path_root_global {};

    struct Path_root : std::variant<Path_root_global, Type_id> {
        using variant::variant, variant::operator=;
    };

    struct Path {
        std::vector<Path_segment> segments;
        std::optional<Path_root>  root;
        kieli::Name               head;

        [[nodiscard]] auto is_simple_name() const noexcept -> bool;
    };

    struct Concept_reference {
        std::optional<std::vector<Template_argument>> template_arguments;
        Path                                          path;
        kieli::Range                                  range;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type_id, Wildcard>;
        kieli::Upper                   name;
        std::vector<Concept_reference> concepts;
        std::optional<Default>         default_argument;
    };

    struct Template_value_parameter {
        using Default = std::variant<Expression_id, Wildcard>;
        kieli::Lower           name;
        std::optional<Type_id> type;
        std::optional<Default> default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        kieli::Lower           name;
        std::optional<Default> default_argument;
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

    using Template_parameters = std::optional<std::vector<Template_parameter>>;

    struct Function_argument {
        Expression_id               expression;
        std::optional<kieli::Lower> name;
    };

    struct Function_parameter {
        Pattern_id                   pattern;
        std::optional<Type_id>       type;
        std::optional<Expression_id> default_argument;
    };

    enum class Loop_source { plain_loop, while_loop, for_loop };
    enum class Conditional_source { if_, elif, while_loop_body };

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

        struct Invocation {
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
            struct Field {
                kieli::Lower  name;
                Expression_id expression;
            };

            Path               constructor_path;
            std::vector<Field> initializers;
        };

        struct Binary_operator_invocation {
            Expression_id     left;
            Expression_id     right;
            kieli::Identifier op;
            kieli::Range      op_range;
        };

        struct Struct_field_access {
            Expression_id base_expression;
            kieli::Lower  field_name;
        };

        struct Tuple_field_access {
            Expression_id              base_expression;
            utl::Explicit<std::size_t> field_index;
            kieli::Range               field_index_range;
        };

        struct Array_index_access {
            Expression_id base_expression;
            Expression_id index_expression;
        };

        struct Method_invocation {
            std::vector<Function_argument>                function_arguments;
            std::optional<std::vector<Template_argument>> template_arguments;
            Expression_id                                 base_expression;
            kieli::Lower                                  method_name;
        };

        struct Conditional {
            Expression_id                     condition;
            Expression_id                     true_branch;
            Expression_id                     false_branch;
            utl::Explicit<Conditional_source> source;
            utl::Explicit<bool>               has_explicit_false_branch;
        };

        struct Match {
            struct Case {
                Pattern_id    pattern;
                Expression_id expression;
            };

            std::vector<Case> cases;
            Expression_id     expression;
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
            Pattern_id             pattern;
            Expression_id          initializer;
            std::optional<Type_id> type;
        };

        struct Local_type_alias {
            kieli::Upper name;
            Type_id      type;
        };

        struct Ret {
            std::optional<Expression_id> expression;
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
            Expression_id expression;
        };

        struct Meta {
            Expression_id expression;
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
        struct Name {
            kieli::Lower name;
            Mutability   mutability;
        };

        struct Field {
            kieli::Lower              name;
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
            using variant::variant, variant::operator=;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            kieli::Upper     name;
            Constructor_body body;
        };

        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> element_patterns;
        };

        struct Alias {
            kieli::Lower name;
            Mutability   mutability;
            Pattern_id   pattern;
        };

        struct Guarded {
            Pattern_id guarded_pattern;
            Expression guard_expression;
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
              pattern::Name,
              pattern::Constructor,
              pattern::Abbreviated_constructor,
              pattern::Tuple,
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
        struct Self {};

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
              type::Implementation,
              type::Template_application> {
        using variant::variant, variant::operator=;
    };

    struct Type {
        Type_variant variant;
        kieli::Range range;
    };

    struct Self_parameter {
        Mutability          mutability;
        utl::Explicit<bool> is_reference;
        kieli::Range        range;
    };

    struct Function_signature {
        Template_parameters             template_parameters;
        std::vector<Function_parameter> function_parameters;
        std::optional<Self_parameter>   self_parameter;
        Type                            return_type;
        kieli::Lower                    name;
    };

    struct Type_signature {
        std::vector<Concept_reference> concepts;
        kieli::Upper                   name;
    };

    namespace definition {
        struct Function {
            Function_signature signature;
            Expression         body;
        };

        struct Field {
            kieli::Lower name;
            Type         type;
            kieli::Range range;
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
            using variant::variant, variant::operator=;
        };

        struct Constructor {
            kieli::Upper     name;
            Constructor_body body;
        };

        struct Enumeration {
            std::vector<Constructor> constructors;
            kieli::Upper             name;
            Template_parameters      template_parameters;
        };

        struct Alias {
            kieli::Upper        name;
            Type                type;
            Template_parameters template_parameters;
        };

        struct Concept {
            std::vector<Function_signature> function_signatures;
            std::vector<Type_signature>     type_signatures;
            kieli::Upper                    name;
            Template_parameters             template_parameters;
        };

        struct Implementation {
            Type                    type;
            std::vector<Definition> definitions;
            Template_parameters     template_parameters;
        };

        struct Submodule {
            std::vector<Definition> definitions;
            kieli::Lower            name;
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
        using variant::variant, variant::operator=;
    };

    struct Definition {
        Definition_variant variant;
        kieli::Source_id   source;
        kieli::Range       range;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression> expressions;
        utl::Index_vector<Pattern_id, Pattern>       patterns;
        utl::Index_vector<Type_id, Type>             types;
    };
} // namespace kieli::ast

struct kieli::AST::Module {
    std::vector<ast::Definition> definitions;
    ast::Arena                   arena;
};
