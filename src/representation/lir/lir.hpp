#pragma once

#include "utl/utilities.hpp"
#include "utl/wrapper.hpp"
#include "vm/virtual_machine.hpp"
#include "representation/token/token.hpp"


/*

    The Low-level Intermediate Representation (LIR) is the lowest level tree
    representation of a program. It contains information that is strictly
    required for bytecode generation. It is produced by lowering the CIR.

*/


namespace lir {

    struct [[nodiscard]] Expression;

    namespace expression {
        template <class T>
        struct Constant {
            T value {};
        };
        // Sequence of things that are all pushed onto the stack. Can represent tuples, array literals, and struct initializers.
        struct Tuple {
            std::vector<Expression> elements;
        };
        // Invocation of a function the address of which is visible from the callsite.
        struct Direct_invocation {
            std::string             function_symbol;
            std::vector<Expression> arguments;
        };
        // Invocation of a function through a pointer the value of which is determined at runtime.
        struct Indirect_invocation {
            utl::Wrapper<Expression> invocable;
            std::vector<Expression>  arguments;
        };
        struct Local_variable_bitcopy {
            vm::Local_offset_type frame_offset {};
            vm::Local_size_type   byte_count   {};
        };
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
        };
        struct Unconditional_jump {
            vm::Local_offset_type target_offset;
        };
        struct Conditional_jump {
            utl::Wrapper<Expression> condition;
            vm::Local_offset_type    target_offset {};
        };
        struct Hole {
            utl::Source_view source_view;
        };
    }


    struct Expression : std::variant<
        expression::Constant<utl::I8>,
        expression::Constant<utl::I16>,
        expression::Constant<utl::I32>,
        expression::Constant<utl::I64>,
        expression::Constant<utl::U8>,
        expression::Constant<utl::U16>,
        expression::Constant<utl::U32>,
        expression::Constant<utl::U64>,
        expression::Constant<compiler::Floating>,
        expression::Constant<compiler::Character>,
        expression::Constant<compiler::Boolean>,
        expression::Constant<compiler::String>,
        expression::Tuple,
        expression::Direct_invocation,
        expression::Indirect_invocation,
        expression::Local_variable_bitcopy,
        expression::Block,
        expression::Unconditional_jump,
        expression::Conditional_jump,
        expression::Hole>
    {
        using variant::variant;
        using variant::operator=;
    };


    struct Function {
        std::string symbol;
        Expression  body;
    };


    using Node_context = utl::Wrapper_context<Expression>;

}