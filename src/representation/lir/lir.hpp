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
            vm::Local_size_type     return_value_size {};
        };
        // Invocation of a function through a pointer the value of which is determined at runtime.
        struct Indirect_invocation {
            utl::Wrapper<Expression> invocable;
            std::vector<Expression>  arguments;
            vm::Local_size_type      return_value_size {};
        };
        struct Local_variable_bitcopy {
            vm::Local_offset_type frame_offset {};
            vm::Local_size_type   byte_count {};
        };
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
            vm::Local_offset_type    result_object_frame_offset {};
            vm::Local_size_type      result_size {};
            vm::Local_size_type      scope_size {};
        };
        struct Loop {
            utl::Wrapper<Expression> body;
        };
        struct Break {
            utl::Wrapper<Expression> result;
        };
        struct Continue {};
        struct Conditional {
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> true_branch;
            utl::Wrapper<Expression> false_branch;
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
        expression::Loop,
        expression::Break,
        expression::Continue,
        expression::Conditional,
        expression::Hole>
    {
        using variant::variant;
        using variant::operator=;
    };


    struct Function {
        std::string symbol;
        Expression  body;
    };


    using Node_arena = utl::Wrapper_arena<Expression>;

}