#pragma once

#include "utl/utilities.hpp"
#include "utl/wrapper.hpp"
#include "lexer/token.hpp"


/*

    The Low-level Intermediate Representation (LIR) is the lowest level tree
    representation of a program. It contains information that is strictly
    required for bytecode generation. It is produced by lowering the CIR.

*/


namespace lir {

    struct [[nodiscard]] Expression;


    // Offset from the beginning of the code section of the current (possibly composite) module. Used for constants such as function addresses.
    struct Module_offset {
        utl::U64 value = 0;
    };

    // Offset from the current frame pointer. Used for local addressing on the stack.
    struct Frame_offset {
        utl::I64 value = 0;
    };

    // Jump offset from the current instruction pointer. Used for local jumps that arise from things like `if` or `loop` expressions.
    struct Local_jump_offset {
        utl::I16 value = 0;
    };


    namespace expression {

        template <class T>
        struct Constant {
            T value;
        };

        // A sequence of things that are all pushed onto the stack. Can represent tuples, array literals, and struct initializers.
        struct Tuple {
            std::vector<Expression> elements;
        };

        // An invocation of a function the address of which is visible from the callsite.
        struct Direct_invocation {
            Module_offset           function;
            std::vector<Expression> arguments;
        };

        // An invocation of a function through a pointer the value of which is determined at runtime.
        struct Indirect_invocation {
            utl::Wrapper<Expression> invocable;
            std::vector<Expression> arguments;
        };

        struct Let_binding {
            utl::Wrapper<Expression> initializer;
        };

        struct Loop {
            utl::Wrapper<Expression> body;
        };

        struct Unconditional_jump {
            Local_jump_offset target_offset;
        };

        struct Conditional_jump {
            utl::Wrapper<Expression> condition;
            Local_jump_offset       target_offset;
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
        expression::Constant<utl::Float>,
        expression::Constant<utl::Char>,
        expression::Constant<bool>,
        expression::Constant<compiler::String>,
        expression::Tuple,
        expression::Direct_invocation,
        expression::Indirect_invocation,
        expression::Let_binding,
        expression::Loop,
        expression::Unconditional_jump,
        expression::Conditional_jump
    > {};


    struct Module {};

}