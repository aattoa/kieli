#include "utl/utilities.hpp"
#include "representation/lir/lir.hpp"
#include "vm/opcode.hpp"
#include "vm/vm_formatting.hpp"
#include "codegen.hpp"


namespace {

    using Function_references = std::vector<compiler::Function_reference>;

    template <class T>
    constexpr auto const_op_for = std::invoke([] {
        switch (sizeof(T)) {
        case 1: return vm::Opcode::const_1;
        case 2: return vm::Opcode::const_2;
        case 4: return vm::Opcode::const_4;
        case 8: return vm::Opcode::const_8;
        default: utl::unreachable();
        }
    });


    struct Expression_codegen_visitor {
        vm::Constants::String::Pool              & string_pool;
        std::vector<vm::Constants::String>       & strings;
        vm::Bytecode                             & code;
        std::vector<compiler::Function_reference>& function_references;

        auto operator()(lir::expression::Constant<compiler::String> const& constant) {
            strings.push_back(string_pool.make(constant.value.view()));
            code.write(vm::Opcode::const_string, strings.size() - 1);
        }
        auto operator()(lir::expression::Constant<compiler::Floating> const& constant) {
            code.write(const_op_for<decltype(compiler::Floating::value)>, constant.value.value);
        }
        auto operator()(lir::expression::Constant<compiler::Character> const& constant) {
            code.write(const_op_for<decltype(compiler::Character::value)>, constant.value.value);
        }
        auto operator()(lir::expression::Constant<compiler::Boolean> const& constant) {
            code.write(constant.value.value ? vm::Opcode::const_true : vm::Opcode::const_false);
        }
        template <std::integral T>
        auto operator()(lir::expression::Constant<T> const& constant) {
            code.write(const_op_for<T>, constant.value);
        }
        auto operator()(lir::expression::Tuple const& tuple) {
            for (lir::Expression const& element : tuple.elements)
                recurse(element);
        }
        auto operator()(lir::expression::Block const& block) {
            if (block.result_size != 0 && !block.side_effect_expressions.empty())
                code.write(vm::Opcode::reserve_stack_space, block.result_size);

            for (lir::Expression const& side_effect : block.side_effect_expressions)
                recurse(side_effect);
            recurse(*block.result_expression);

            if (block.result_size != 0 && !block.side_effect_expressions.empty())
                code.write(vm::Opcode::bitcopy_to_local, block.result_size, block.result_object_frame_offset);

            if (block.side_effect_expressions.empty())
                return;

            switch (block.scope_size) {
            case 0: break;
            case 1: code.write(vm::Opcode::pop_1); break;
            case 2: code.write(vm::Opcode::pop_2); break;
            case 4: code.write(vm::Opcode::pop_4); break;
            case 8: code.write(vm::Opcode::pop_8); break;
            default: code.write(vm::Opcode::pop_n, block.scope_size); break;
            }
        }
        auto operator()(lir::expression::Direct_invocation const& invocation) {
            for (lir::Expression const& argument : invocation.arguments)
                recurse(argument);
            code.write(invocation.return_value_size == 0 ? vm::Opcode::call_0 : vm::Opcode::call);
            auto const offset = code.current_offset();
            code.write(0_uz);
            function_references.push_back(compiler::Function_reference {
                .symbol = invocation.function_symbol,
                .code_offset = offset
            });
        }
        auto operator()(lir::expression::Indirect_invocation const& invocation) {
            for (lir::Expression const& argument : invocation.arguments)
                recurse(argument);
            recurse(*invocation.invocable);
            code.write(invocation.return_value_size == 0 ? vm::Opcode::call_ptr_0 : vm::Opcode::call_ptr);
        }
        auto operator()(lir::expression::Local_variable_bitcopy const& local) {
            code.write(vm::Opcode::bitcopy_from_local, local.byte_count, local.frame_offset);
        }
        auto operator()(lir::expression::Conditional const& conditional) {
            recurse(*conditional.condition);
            auto const jump_to_false_branch = code.reserve_slots_for<vm::Opcode, vm::Local_offset_type>();

            recurse(*conditional.true_branch);
            auto const jump_over_false_branch = code.reserve_slots_for<vm::Opcode, vm::Local_offset_type>();

            auto const false_branch_start_offset = code.current_offset();
            recurse(*conditional.false_branch);
            auto const false_branch_end_offset = code.current_offset();


            jump_to_false_branch.write_to_reserved(
                vm::Opcode::local_jump_false,
                utl::safe_cast<vm::Local_offset_type>(
                    false_branch_start_offset - jump_to_false_branch.offset - sizeof(vm::Local_offset_type) - 1));

            jump_over_false_branch.write_to_reserved(
                vm::Opcode::local_jump,
                utl::safe_cast<vm::Local_offset_type>(
                    false_branch_end_offset - jump_over_false_branch.offset - sizeof(vm::Local_offset_type) - 1));
        }

        auto operator()(auto const&) -> void {
            utl::todo();
        }

        auto recurse(lir::Expression const& expression) -> void {
            return std::visit(*this, expression);
        }
    };

}


auto compiler::codegen(Lower_result&& lower_result) -> Codegen_result {
    Codegen_result codegen_result;

    for (lir::Function const& function : lower_result.functions) {
        std::visit(Expression_codegen_visitor {
            codegen_result.string_pool,
            codegen_result.strings,
            codegen_result.bytecode,
            codegen_result.function_references
        }, function.body);
    }

    fmt::print("\n\n{}\n\n", codegen_result.bytecode);

    utl::todo();
}
