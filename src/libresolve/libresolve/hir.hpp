#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/index_vector.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libphase/phase.hpp>
#include <libresolve/fwd.hpp>

namespace libresolve {
    struct Template_parameter_tag : utl::Vector_index<Template_parameter_tag> {
        using Vector_index::Vector_index;
    };

    struct Type_variable_tag : utl::Vector_index<Type_variable_tag> {
        using Vector_index::Vector_index;
    };

    struct Mutability_variable_tag : utl::Vector_index<Mutability_variable_tag> {
        using Vector_index::Vector_index;
    };

    struct Local_variable_tag : utl::Vector_index<Local_variable_tag> {
        using Vector_index::Vector_index;
    };
} // namespace libresolve

namespace libresolve::hir {

    struct Class_reference {
        utl::Mutable_wrapper<Typeclass_info> info;
        utl::Source_range                    source_range;
    };

    enum class Type_variable_kind { general, integral };

    namespace mutability {
        using Concrete = kieli::Mutability;

        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Mutability_variable_tag tag;
        };
    } // namespace mutability

    struct Mutability {
        using Variant = std::variant<
            mutability::Concrete, //
            mutability::Parameterized,
            mutability::Variable>;
        utl::Mutable_wrapper<Variant> variant;
        utl::Source_range             source_range;
    };

    struct Type {
        struct Variant;

        utl::Mutable_wrapper<Variant> variant;
        utl::Source_range             source_range;
    };

    struct Function_argument {
        utl::Wrapper<Expression>         expression;
        std::optional<kieli::Name_lower> name;
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
            Local_variable_tag variable_tag;
            kieli::Identifier  identifier;
            Mutability         mutability;
        };

        struct Alias {
            Name                  name;
            utl::Wrapper<Pattern> aliased_pattern;
        };

        struct Guarded {
            utl::Wrapper<Pattern>    guarded_pattern;
            utl::Wrapper<Expression> guard_expression;
        };
    } // namespace pattern

    struct Pattern {
        using Variant = std::variant<
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
            pattern::Guarded>;
        Variant           variant;
        Type              type;
        utl::Source_range source_range;
    };

    namespace type {
        struct Array {
            Type                     element_type;
            utl::Wrapper<Expression> length;
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
            utl::Wrapper<Enumeration_info> info;
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
            Type_variable_tag tag;
        };

        struct Error {};
    } // namespace type

    struct Type::Variant
        : std::variant<
              kieli::built_in_type::Integer,
              kieli::built_in_type::Floating,
              kieli::built_in_type::Character,
              kieli::built_in_type::Boolean,
              kieli::built_in_type::String,
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
        using variant::variant;
        using variant::operator=;
    };

    namespace expression {
        struct Array_literal {
            std::vector<Expression> elements;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            utl::Wrapper<Expression> body;
        };

        struct Break {
            utl::Wrapper<Expression> result;
        };

        struct Continue {};

        struct Block {
            std::vector<Expression>  side_effects;
            utl::Wrapper<Expression> result;
        };

        struct Let_binding {
            utl::Wrapper<Pattern>    pattern;
            Type                     type;
            utl::Wrapper<Expression> initializer;
        };

        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> expression;
            };

            std::vector<Case>        cases;
            utl::Wrapper<Expression> expression;
        };

        struct Variable_reference {
            Local_variable_tag tag;
            kieli::Identifier  identifier;
        };

        struct Function_reference {
            utl::Mutable_wrapper<Function_info> info;
        };

        struct Indirect_invocation {
            utl::Wrapper<Expression>       function;
            std::vector<Function_argument> arguments;
        };

        struct Direct_invocation {
            utl::Wrapper<Function_info>    function_info;
            std::vector<Function_argument> arguments;
        };

        struct Sizeof {
            Type inspected_type;
        };

        struct Addressof {
            Mutability               mutability;
            utl::Wrapper<Expression> lvalue_expression;
        };

        struct Hole {};

        struct Error {};
    } // namespace expression

    struct Expression {
        using Variant = std::variant<
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
            expression::Indirect_invocation,
            expression::Direct_invocation,
            expression::Sizeof,
            expression::Addressof,
            expression::Hole,
            expression::Error>;
        Variant           variant;
        Type              type;
        utl::Source_range source_range;
    };

    using Node_arena = utl::Wrapper_arena<Expression, Pattern, Type::Variant, Mutability::Variant>;

    using Template_argument = std::variant<Expression, Type, Mutability>;

    struct Template_type_parameter {
        std::vector<Class_reference> classes;
        kieli::Name_upper            name;
    };

    struct Template_mutability_parameter {
        kieli::Name_lower name;
    };

    struct Template_value_parameter {
        Type              type;
        kieli::Name_lower name;
    };

    struct Template_parameter {
        using Variant = std::variant<
            Template_type_parameter,
            Template_mutability_parameter,
            Template_value_parameter>;

        Variant                variant;
        Template_parameter_tag tag;
        utl::Source_range      source_range;
    };

    struct Function_parameter {
        Pattern pattern;
        Type    type;
    };

    struct Function_signature {
        std::vector<Function_parameter> parameters;
        Type                            return_type;
        Type                            function_type;
    };

    struct Function {
        Function_signature signature;
        Expression         body;
    };

    struct Enumeration {
        // TODO
    };

    struct Alias {
        kieli::Name_upper name;
        Type              type;
    };

    struct Typeclass {
        // TODO
    };

    struct Module {
        utl::Mutable_wrapper<Environment> environment;
    };

    auto format_to(Expression const&, std::string&) -> void;
    auto format_to(Pattern const&, std::string&) -> void;
    auto format_to(Type const&, std::string&) -> void;
    auto format_to(Mutability const&, std::string&) -> void;

    auto to_string(auto const& x) -> std::string
        requires requires(std::string out) { hir::format_to(x, out); }
    {
        std::string output;
        hir::format_to(x, output);
        return output;
    };

} // namespace libresolve::hir
