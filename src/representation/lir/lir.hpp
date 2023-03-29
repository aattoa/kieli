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

    // TODO: replace with something more efficient
    using Symbol = std::string;

    struct [[nodiscard]] Expression;


    // Offset from the current frame pointer. Used for local addressing on the stack.
    struct Frame_offset {
        utl::I64 value {};
    };

    // Jump offset from the current instruction pointer. Used for local jumps that arise from things like `if` or `loop` expressions.
    struct Local_jump_offset {
        vm::Local_offset_type value {};
    };


    namespace expression {
        template <class T>
        struct Constant {
            T value;
        };
        // Sequence of things that are all pushed onto the stack. Can represent tuples, array literals, and struct initializers.
        struct Tuple {
            std::vector<Expression> elements;
        };
        // Invocation of a function the address of which is visible from the callsite.
        struct Direct_invocation {
            Symbol                  function_symbol;
            std::vector<Expression> arguments;
        };
        // Invocation of a function through a pointer the value of which is determined at runtime.
        struct Indirect_invocation {
            utl::Wrapper<Expression> invocable;
            std::vector<Expression>  arguments;
        };
        struct Unconditional_jump {
            Local_jump_offset target_offset;
        };
        struct Conditional_jump {
            utl::Wrapper<Expression> condition;
            Local_jump_offset        target_offset;
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
        expression::Unconditional_jump,
        expression::Conditional_jump,
        expression::Hole>
    {
        using variant::variant;
        using variant::operator=;
    };


    struct Module {};

}