#pragma once

#include <libresolve/mir.hpp>
#include <libvm/virtual_machine.hpp>
#include <libutl/common/safe_integer.hpp>


/*

    The Concrete Intermediate Representation (CIR) is a fully typed, concrete
    representation of a program, which means that it contains no information
    about generics or type variables. It is produced by reifying the MIR.

*/


namespace cir {

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;


    struct [[nodiscard]] Type {
        struct [[nodiscard]] Variant;
        using Size = utl::Safe_integer<vm::Local_size_type>;
        utl::Wrapper<Variant> value;
        Size                  size;
        utl::Source_view      source_view;
    };

    namespace type {
        using mir::type::Primitive;
        using mir::type::Integer;
        using mir::type::Floating;
        using mir::type::Character;
        using mir::type::Boolean;
        using mir::type::String;
        struct Tuple {
            std::vector<Type> field_types;
        };
        struct Struct_reference {

        };
        struct Enum_reference {

        };
        // Can represent both pointers and references
        struct Pointer {
            Type pointed_to_type;
        };
    }

    struct Type::Variant : std::variant<
        type::Integer,
        type::Floating,
        type::Character,
        type::Boolean,
        type::String,
        type::Tuple,
        type::Struct_reference,
        type::Enum_reference,
        type::Pointer>
    {
        using variant::variant;
        using variant::operator=;
    };



    namespace pattern {
        template <class T>
        struct Literal {
            T value;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Exhaustive {};
    }

    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<compiler::Signed_integer>,
            pattern::Literal<compiler::Unsigned_integer>,
            pattern::Literal<compiler::Integer_of_unknown_sign>,
            pattern::Literal<compiler::Floating>,
            pattern::Literal<compiler::Character>,
            pattern::Literal<compiler::Boolean>,
            pattern::Literal<compiler::String>,
            pattern::Tuple,
            pattern::Exhaustive
        >;
        Variant          value;
        Type             type; // TODO: is this field necessary?
        utl::Source_view source_view;
    };



    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
            Type::Size               scope_size {};
            vm::Local_offset_type    result_object_frame_offset {};
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
            vm::Local_offset_type frame_offset {};
            compiler::Identifier  identifier;
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
            expression::Literal<compiler::Signed_integer>,
            expression::Literal<compiler::Unsigned_integer>,
            expression::Literal<compiler::Integer_of_unknown_sign>,
            expression::Literal<compiler::Floating>,
            expression::Literal<compiler::Character>,
            expression::Literal<compiler::Boolean>,
            expression::Literal<compiler::String>,
            expression::Block,
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Let_binding,
            expression::Local_variable_reference,
            expression::Conditional,
            expression::Hole
        >;
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

}


DECLARE_FORMATTER_FOR(cir::Expression);
DECLARE_FORMATTER_FOR(cir::Pattern);
DECLARE_FORMATTER_FOR(cir::Type);
