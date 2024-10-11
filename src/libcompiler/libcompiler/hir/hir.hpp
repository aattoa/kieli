#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libcompiler/compiler.hpp>

namespace kieli::hir {
    enum class Type_variable_kind { general, integral };

    enum class Expression_kind { place, value };

    struct Template_parameter_tag : utl::Explicit<std::size_t> {
        using Explicit::Explicit;
    };

    struct Local_variable_tag : utl::Explicit<std::size_t> {
        using Explicit::Explicit;
    };

    struct Error {};

    struct Wildcard {};

    struct Mutability {
        Mutability_id id;
        kieli::Range  range;
    };

    struct Type {
        Type_id      id;
        kieli::Range range;
    };

    struct Match_case {
        Pattern_id    pattern;
        Expression_id expression;
    };

    namespace pattern {
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
              Wildcard,
              pattern::Tuple,
              pattern::Slice,
              pattern::Name,
              pattern::Alias,
              pattern::Guarded> {
        using variant::variant;
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
            Expression_id              invocable;
            std::vector<Expression_id> arguments;
        };

        struct Direct_function_call {
            kieli::Lower               function_name;
            Function_id                function_id;
            std::vector<Expression_id> arguments;
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
    } // namespace expression

    struct Expression_variant
        : std::variant<
              Error,
              Wildcard,
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
              expression::Defer> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant variant;
        Type_id            type;
        Expression_kind    kind;
        kieli::Range       range;
    };

    namespace type {
        enum class Integer { i8, i16, i32, i64, u8, u16, u32, u64 };

        struct Floating {};

        struct Boolean {};

        struct Character {};

        struct String {};

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
    } // namespace type

    struct Type_variant
        : std::variant<
              Error,
              type::Integer,
              type::Floating,
              type::Character,
              type::Boolean,
              type::String,
              type::Array,
              type::Slice,
              type::Reference,
              type::Pointer,
              type::Function,
              type::Enumeration,
              type::Tuple,
              type::Parameterized,
              type::Variable> {
        using variant::variant;
    };

    namespace mutability {
        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Mutability_variable_id id;
        };
    } // namespace mutability

    struct Mutability_variant
        : std::variant<Error, kieli::Mutability, mutability::Parameterized, mutability::Variable> {
        using variant::variant;
    };

    struct Template_argument : std::variant<Expression, Type, Mutability> {
        using variant::variant;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type, Wildcard>;
        std::vector<Concept_id> concept_ids;
        kieli::Upper            name;
        std::optional<Default>  default_argument;
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
        using variant::variant;
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
        Scope_id                        scope_id;
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

    // Get the name of a built-in integer type.
    auto integer_name(type::Integer type) -> std::string_view;

    // Get the type of an expression.
    auto expression_type(Expression const& expression) -> hir::Type;

    // Get the type of a pattern.
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
