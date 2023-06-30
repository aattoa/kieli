#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/common/safe_integer.hpp>
#include <liblex/token.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


/*

    The Concrete Intermediate Representation (CIR) is a fully typed, concrete
    representation of a program, which means that it contains no information
    about generics or type variables. It is produced by reifying the HIR.

*/


namespace cir {

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;


    struct [[nodiscard]] Type {
        struct [[nodiscard]] Variant;
        utl::Wrapper<Variant> value;
        utl::Safe_usize       size;
        utl::Source_view      source_view;
    };

    namespace type {
        struct Tuple {
            std::vector<Type> field_types;
        };
        struct Struct_reference {
            // TODO
        };
        struct Enum_reference {
            // TODO
        };
        // Can represent both pointers and references
        struct Pointer {
            Type pointed_to_type;
        };
    }

    struct Type::Variant : std::variant<
        compiler::built_in_type::Integer,
        compiler::built_in_type::Floating,
        compiler::built_in_type::Character,
        compiler::built_in_type::Boolean,
        compiler::built_in_type::String,
        type::Tuple,
        type::Struct_reference,
        type::Enum_reference,
        type::Pointer>
    {
        using variant::variant;
        using variant::operator=;
    };



    namespace pattern {
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Exhaustive {};
    }

    struct Pattern {
        using Variant = std::variant<
            compiler::Integer,
            compiler::Floating,
            compiler::Character,
            compiler::Boolean,
            compiler::String,
            pattern::Tuple,
            pattern::Exhaustive>;

        Variant          value;
        Type             type; // TODO: is this field necessary?
        utl::Source_view source_view;
    };



    namespace expression {
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
            utl::Safe_usize          scope_size;
            utl::Safe_isize          result_object_frame_offset;
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
        struct Let_binding {
            Pattern                  pattern;
            utl::Wrapper<Expression> initializer;
        };
        struct Local_variable_reference {
            utl::Safe_isize    frame_offset;
            utl::Pooled_string identifier;
        };
        struct Conditional {
            utl::Wrapper<cir::Expression> condition;
            utl::Wrapper<cir::Expression> true_branch;
            utl::Wrapper<cir::Expression> false_branch;
        };
        struct Hole {};
    }

    struct [[nodiscard]] Expression {
        using Variant = std::variant<
            compiler::Integer,
            compiler::Floating,
            compiler::Character,
            compiler::Boolean,
            compiler::String,
            expression::Block,
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Let_binding,
            expression::Local_variable_reference,
            expression::Conditional,
            expression::Hole>;

        Variant          value;
        Type             type;
        utl::Source_view source_view;
    };


    struct Function {
        std::string       symbol;
        std::vector<Type> parameter_types;
        Expression        body;
    };


    using Node_arena = utl::Wrapper_arena<Expression, Pattern, Type::Variant>;


    auto format_to(Expression const&, std::string&) -> void;
    auto format_to(Pattern    const&, std::string&) -> void;
    auto format_to(Type       const&, std::string&) -> void;

    auto to_string(auto const& x) -> std::string
        requires requires { cir::format_to(x, std::declval<std::string&>()); }
    {
        std::string output;
        cir::format_to(x, output);
        return output;
    }

}
