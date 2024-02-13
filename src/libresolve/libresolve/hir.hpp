#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libphase/phase.hpp>
#include <libresolve/fwd.hpp>

namespace libresolve::hir {

    template <class Derived>
    struct Strong_tag_base {
        std::size_t value;

        explicit constexpr Strong_tag_base(std::size_t const value) noexcept : value { value } {}

        [[nodiscard]] constexpr auto operator==(Strong_tag_base const&) const -> bool = default;
    };

    struct Template_parameter_tag : Strong_tag_base<Template_parameter_tag> {
        using Strong_tag_base::Strong_tag_base;
    };

    struct Unification_variable_tag : Strong_tag_base<Unification_variable_tag> {
        using Strong_tag_base::Strong_tag_base;
    };

    struct Local_variable_tag : Strong_tag_base<Local_variable_tag> {
        using Strong_tag_base::Strong_tag_base;
    };

    enum class Unification_type_variable_kind { general, integral };

    struct Mutability {
        struct Concrete {
            utl::Explicit<bool> is_mutable;
        };

        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Unification_variable_tag tag;
        };

        using Variant = std::variant<Concrete, Parameterized, Variable>;

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
            std::vector<Pattern> patterns;
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
            utl::Wrapper<Pattern> pattern;
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

        struct Unification_variable {
            utl::Mutable_wrapper<Unification_type_variable_state> state;
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
              type::Unification_variable,
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
            utl::Wrapper<Function_info> info;
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

        struct Address {
            utl::Wrapper<Expression> lvalue;
        };

        struct Reference {
            utl::Wrapper<Expression> lvalue;
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
            expression::Address,
            expression::Reference,
            expression::Hole>;
        Variant           variant;
        Type              type;
        utl::Source_range source_range;
    };

    using Node_arena = utl::Wrapper_arena<Mutability::Variant, Type::Variant, Pattern, Expression>;

    struct Function {
        struct Parameter {
            Pattern pattern;
            Type    type;
        };

        struct Signature {
            std::vector<Parameter> parameters;
            Type                   return_type;
        };

        Signature  signature;
        Expression body;
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

} // namespace libresolve::hir
