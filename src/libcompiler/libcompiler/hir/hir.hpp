#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libcompiler/compiler.hpp>

namespace kieli::hir {
    struct [[nodiscard]] Mutability;
    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;

    struct Template_parameter_tag : utl::Explicit<std::size_t> {
        using Explicit::Explicit;
    };

    struct Local_variable_tag : utl::Explicit<std::size_t> {
        using Explicit::Explicit;
    };

    struct Expression_id : utl::Vector_index<Expression_id> {
        using Vector_index::Vector_index;
    };

    struct Pattern_id : utl::Vector_index<Pattern_id> {
        using Vector_index::Vector_index;
    };

    struct Type_id : utl::Vector_index<Type_id> {
        using Vector_index::Vector_index;
    };

    struct Mutability_id : utl::Vector_index<Mutability_id> {
        using Vector_index::Vector_index;
    };

    struct Type_variable_id : utl::Vector_index<Type_variable_id> {
        using Vector_index::Vector_index;
    };

    struct Mutability_variable_id : utl::Vector_index<Mutability_variable_id> {
        using Vector_index::Vector_index;
    };

    struct Module_id : utl::Vector_index<Module_id> {
        using Vector_index::Vector_index;
    };

    struct Environment_id : utl::Vector_index<Environment_id> {
        using Vector_index::Vector_index;
    };

    struct Function_id : utl::Vector_index<Function_id> {
        using Vector_index::Vector_index;
    };

    struct Enumeration_id : utl::Vector_index<Enumeration_id> {
        using Vector_index::Vector_index;
    };

    struct Alias_id : utl::Vector_index<Alias_id> {
        using Vector_index::Vector_index;
    };

    struct Concept_id : utl::Vector_index<Concept_id> {
        using Vector_index::Vector_index;
    };

    enum class Type_variable_kind { general, integral };

    enum class Expression_kind { place, value };

    struct Mutability {
        Mutability_id id;
        kieli::Range  range;
    };

    struct Type {
        Type_id      id;
        kieli::Range range;
    };

    struct Concept_reference {
        Concept_id   id;
        kieli::Upper name;
    };

    struct Match_case {
        Pattern_id    pattern;
        Expression_id expression;
    };

    struct Function_argument {
        Expression_id               expression;
        std::optional<kieli::Lower> name;
    };

    namespace pattern {
        struct Wildcard {};

        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> patterns;
        };

        struct Name {
            Mutability         mutability;
            kieli::Identifier  identifier;
            Local_variable_tag variable_tag;
        };

        struct Alias {
            Mutability         mutability;
            kieli::Identifier  identifier;
            Local_variable_tag variable_tag;
            Pattern_id         pattern;
        };

        struct Guarded {
            Pattern_id    guarded_pattern;
            Expression_id guard_expression;
        };
    } // namespace pattern

    struct Pattern_variant
        : std::variant<
              kieli::Integer,
              kieli::Floating,
              kieli::Character,
              kieli::Boolean,
              kieli::String,
              pattern::Wildcard,
              pattern::Tuple,
              pattern::Slice,
              pattern::Name,
              pattern::Alias,
              pattern::Guarded> {
        using variant::variant, variant::operator=;
    };

    struct Pattern {
        Pattern_variant variant;
        Type_id         type;
        kieli::Range    range;
    };

    namespace expression {
        struct Array_literal {
            std::vector<Expression> elements;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            Expression_id body;
        };

        struct Break {
            Expression_id result;
        };

        struct Continue {};

        struct Block {
            std::vector<Expression> side_effects;
            Expression_id           result;
        };

        struct Let_binding {
            Pattern_id    pattern;
            Type          type;
            Expression_id initializer;
        };

        struct Match {
            std::vector<Match_case> cases;
            Expression_id           expression;
        };

        struct Variable_reference {
            kieli::Lower       name;
            Local_variable_tag tag;
        };

        struct Function_reference {
            kieli::Lower name;
            Function_id  id;
        };

        struct Indirect_function_call {
            Expression_id                  invocable;
            std::vector<Function_argument> arguments;
        };

        struct Direct_function_call {
            kieli::Lower                   function_name;
            Function_id                    function_id;
            std::vector<Function_argument> arguments;
        };

        struct Sizeof {
            Type inspected_type;
        };

        struct Addressof {
            Mutability    mutability;
            Expression_id place_expression;
        };

        struct Dereference {
            Expression_id reference_expression;
        };

        struct Defer {
            Expression_id effect_expression;
        };

        struct Hole {};

        struct Error {};
    } // namespace expression

    struct Expression_variant
        : std::variant<
              kieli::Integer,
              kieli::Floating,
              kieli::Character,
              kieli::Boolean,
              kieli::String,
              expression::Array_literal,
              expression::Tuple,
              expression::Loop,
              expression::Break,
              expression::Continue,
              expression::Block,
              expression::Let_binding,
              expression::Match,
              expression::Variable_reference,
              expression::Function_reference,
              expression::Indirect_function_call,
              expression::Direct_function_call,
              expression::Sizeof,
              expression::Addressof,
              expression::Dereference,
              expression::Defer,
              expression::Hole,
              expression::Error> {
        using variant::variant, variant::operator=;
    };

    struct Expression {
        Expression_variant variant;
        Type_id            type;
        Expression_kind    kind;
        kieli::Range       range;
    };

    namespace type {
        struct Array {
            Type          element_type;
            Expression_id length;
        };

        struct Slice {
            Type element_type;
        };

        struct Tuple {
            std::vector<Type> types;
        };

        struct Function {
            std::vector<Type> parameter_types;
            Type              return_type;
        };

        struct Enumeration {
            kieli::Upper   name;
            Enumeration_id id;
        };

        struct Reference {
            Type       referenced_type;
            Mutability mutability;
        };

        struct Pointer {
            Type       pointee_type;
            Mutability mutability;
        };

        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Type_variable_id id;
        };

        struct Error {};
    } // namespace type

    struct Type_variant
        : std::variant<
              kieli::type::Integer,
              kieli::type::Floating,
              kieli::type::Character,
              kieli::type::Boolean,
              kieli::type::String,
              type::Array,
              type::Slice,
              type::Reference,
              type::Pointer,
              type::Function,
              type::Enumeration,
              type::Tuple,
              type::Parameterized,
              type::Variable,
              type::Error> {
        using variant::variant, variant::operator=;
    };

    namespace mutability {
        using Concrete = kieli::Mutability;

        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Mutability_variable_id id;
        };

        struct Error {};
    } // namespace mutability

    struct Mutability_variant
        : std::variant<
              mutability::Concrete,
              mutability::Parameterized,
              mutability::Variable,
              mutability::Error> {
        using variant::variant, variant::operator=;
    };

    struct Template_argument : std::variant<Expression, Type, Mutability> {
        using variant::variant, variant::operator=;
    };

    struct Wildcard {
        kieli::Range range;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type, Wildcard>;
        std::vector<Concept_reference> concepts;
        kieli::Upper                   name;
        std::optional<Default>         default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        kieli::Lower           name;
        std::optional<Default> default_argument;
    };

    struct Template_value_parameter {
        Type         type;
        kieli::Lower name;
    };

    struct Template_parameter_variant
        : std::variant<
              Template_type_parameter,
              Template_mutability_parameter,
              Template_value_parameter> {
        using variant::variant, variant::operator=;
    };

    struct Template_parameter {
        Template_parameter_variant variant;
        Template_parameter_tag     tag;
        kieli::Range               range;
    };

    struct Function_parameter {
        Pattern                   pattern;
        Type                      type;
        std::optional<Expression> default_argument;
    };

    struct Function_signature {
        std::vector<Template_parameter> template_paramters;
        std::vector<Function_parameter> parameters;
        Type                            return_type;
        Type                            function_type;
        kieli::Lower                    name;
    };

    struct Function {
        Function_signature signature;
        Expression         body;
    };

    struct Enumeration {
        // TODO
    };

    struct Alias {
        kieli::Upper name;
        Type         type;
    };

    struct Concept {
        // TODO
    };

    struct Module {
        Environment_id environment;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression>         expressions;
        utl::Index_vector<Pattern_id, Pattern>               patterns;
        utl::Index_vector<Type_id, Type_variant>             types;
        utl::Index_vector<Mutability_id, Mutability_variant> mutabilities;
    };

    auto expression_type(Expression const& expression) -> hir::Type;
    auto pattern_type(Pattern const& pattern) -> hir::Type;

    auto format(Arena const&, Pattern const&, std::string&) -> void;
    auto format(Arena const&, Expression const&, std::string&) -> void;
    auto format(Arena const&, Type const&, std::string&) -> void;
    auto format(Arena const&, Type_variant const&, std::string&) -> void;
    auto format(Arena const&, Mutability const&, std::string&) -> void;
    auto format(Arena const&, Mutability_variant const&, std::string&) -> void;

    auto to_string(Arena const& arena, auto const& x) -> std::string
        requires requires(std::string output) { hir::format(arena, x, output); }
    {
        std::string output;
        hir::format(arena, x, output);
        return output;
    };
} // namespace kieli::hir
