#pragma once

#include "representation/mir/mir.hpp"
#include "utl/safe_integer.hpp"


/*

    The Concrete Intermediate Representation (CIR) is a fully typed, concrete
    representation of a program, which means that it contains no information
    about generics or type variables. It is produced by reifying the MIR.

*/


namespace cir {

    struct [[nodiscard]] Type {
        struct [[nodiscard]] Variant;
        utl::Wrapper<Variant> value;
        utl::Safe_usize       size;
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

    struct Type::Variant :
        std::variant<
            type::Integer,
            type::Floating,
            type::Character,
            type::Boolean,
            type::String,
            type::Tuple,
            type::Struct_reference,
            type::Enum_reference,
            type::Pointer
        >
    {
        using variant::variant;
        using variant::operator=;
    };



    namespace pattern {
        // TODO
    }

    struct [[nodiscard]] Pattern {
        struct Variant {};
        Variant         value;
        utl::Source_view source_view;
    };



    namespace expression {
        template <class T>
        struct Literal { T value; };
    }

    struct [[nodiscard]] Expression {
        using Variant = std::variant<
            expression::Literal<bool>,
            expression::Literal<utl::Isize>,
            expression::Literal<utl::Float>
        >;
        Variant         value;
        Type            type;
        utl::Source_view source_view;
    };


    struct Function {
        std::vector<Type> parameter_types;
        Expression        body;
    };


    using Node_context = utl::Wrapper_context<Expression, Pattern, Type::Variant>;


    struct Program {};

}