#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/common/safe_integer.hpp>

/*

    The Low-level Intermediate Representation (LIR) is the lowest level tree
    representation of a program. It contains information that is strictly
    required for bytecode generation. It is produced by lowering the CIR.

*/

namespace lir {

    struct [[nodiscard]] Expression;

    namespace expression {
        // Sequence of things that are all pushed onto the stack. Can represent tuples, array
        // literals, and struct initializers.
        struct Tuple {
            std::vector<Expression> elements;
        };

        // Invocation of a function the address of which is visible from the callsite.
        struct Direct_invocation {
            std::string             function_symbol;
            std::vector<Expression> arguments;
            utl::Safe_usize         return_value_size;
        };

        // Invocation of a function through a pointer the value of which is determined at runtime.
        struct Indirect_invocation {
            utl::Wrapper<Expression> invocable;
            std::vector<Expression>  arguments;
            utl::Safe_usize          return_value_size;
        };

        struct Local_variable_bitcopy {
            utl::Safe_isize frame_offset;
            utl::Safe_usize byte_count;
        };

        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
            utl::Safe_isize          result_object_frame_offset;
            utl::Safe_usize          result_size;
            utl::Safe_usize          scope_size;
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
    } // namespace expression

    struct Expression
        : std::variant<
              utl::I8,
              utl::I16,
              utl::I32,
              utl::I64,
              utl::U8,
              utl::U16,
              utl::U32,
              utl::U64,
              kieli::Floating,
              kieli::Character,
              kieli::Boolean,
              kieli::String,
              expression::Tuple,
              expression::Direct_invocation,
              expression::Indirect_invocation,
              expression::Local_variable_bitcopy,
              expression::Block,
              expression::Loop,
              expression::Break,
              expression::Continue,
              expression::Conditional,
              expression::Hole> {
        using variant::variant;
        using variant::operator=;
    };

    struct Function {
        std::string symbol;
        Expression  body;
    };

    using Node_arena = utl::Wrapper_arena<Expression>;

} // namespace lir
