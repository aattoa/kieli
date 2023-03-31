#include "utl/utilities.hpp"
#include "virtual_machine.hpp"
#include "opcode.hpp"
#include "vm_formatting.hpp"


namespace {

    using VM = vm::Virtual_machine;

    using String = vm::Constants::String;


    template <utl::trivial T>
    auto push(VM& vm) -> void {
        if constexpr (std::same_as<T, String>) {
            vm.stack.push(vm.program.constants.string_pool[vm.extract_argument<utl::Usize>()]);
        }
        else {
            vm.stack.push(vm.extract_argument<T>());
        }
    }

    template <bool value>
    auto push_bool(VM& vm) -> void {
        vm.stack.push(value);
    }

    template <utl::trivial T>
    auto dup(VM& vm) -> void {
        vm.stack.push(vm.stack.top<T>());
    }

    template <utl::trivial T>
    auto print(VM& vm) -> void {
        auto const popped = vm.stack.pop<T>();

        if constexpr (std::same_as<T, String>) {
            vm.output_buffer.insert(
                vm.output_buffer.end(),
                popped.pointer,
                popped.pointer + popped.length
            );
        }
        else {
            fmt::format_to(std::back_inserter(vm.output_buffer), "{}\n", popped);
        }

        vm.flush_output(); // Adjust flush frequency later
    }

    template <class T, template <class> class F>
    auto binary_op(VM& vm) -> void {
        auto const right = vm.stack.pop<T>();
        auto const left  = vm.stack.pop<T>();
        vm.stack.push(F<T>{}(left, right));
    }

    template <class T, template <class> class F>
    auto immediate_binary_op(VM& vm) -> void {
        auto const right = vm.stack.pop<T>();
        auto const left  = vm.extract_argument<T>();
        vm.stack.push(F<T>{}(left, right));
    }

    template <class T> constexpr auto add = binary_op<T, std::plus>;
    template <class T> constexpr auto sub = binary_op<T, std::minus>;
    template <class T> constexpr auto mul = binary_op<T, std::multiplies>;
    template <class T> constexpr auto div = binary_op<T, std::divides>;

    template <class T> constexpr auto eq  = binary_op<T, std::equal_to>;
    template <class T> constexpr auto neq = binary_op<T, std::not_equal_to>;
    template <class T> constexpr auto lt  = binary_op<T, std::less>;
    template <class T> constexpr auto lte = binary_op<T, std::less_equal>;
    template <class T> constexpr auto gt  = binary_op<T, std::greater>;
    template <class T> constexpr auto gte = binary_op<T, std::greater_equal>;

    // template <class T> constexpr auto add_i = immediate_binary_op<T, std::equal_to>;
    // template <class T> constexpr auto sub_i = immediate_binary_op<T, std::minus>;
    // template <class T> constexpr auto mul_i = immediate_binary_op<T, std::multiplies>;
    // template <class T> constexpr auto div_i = immediate_binary_op<T, std::divides>;

    template <class T> constexpr auto eq_i  = immediate_binary_op<T, std::equal_to>;
    template <class T> constexpr auto neq_i = immediate_binary_op<T, std::not_equal_to>;
    template <class T> constexpr auto lt_i  = immediate_binary_op<T, std::less>;
    template <class T> constexpr auto lte_i = immediate_binary_op<T, std::less_equal>;
    template <class T> constexpr auto gt_i  = immediate_binary_op<T, std::greater>;
    template <class T> constexpr auto gte_i = immediate_binary_op<T, std::greater_equal>;

    constexpr auto land = binary_op<bool, std::logical_and>;
    constexpr auto lor  = binary_op<bool, std::logical_or>;

    auto lnand(VM& vm) -> void {
        vm.stack.push(!(vm.stack.pop<bool>() && vm.stack.pop<bool>()));
    }

    auto lnor(VM& vm) -> void {
        vm.stack.push(!(vm.stack.pop<bool>() || vm.stack.pop<bool>()));
    }

    auto lnot(VM& vm) -> void {
        vm.stack.push(!vm.stack.pop<bool>());
    }

    template <utl::trivial From, utl::trivial To>
    auto cast(VM& vm) -> void {
        vm.stack.push(static_cast<To>(vm.stack.pop<From>()));
    }

    auto iinc_top(VM& vm) -> void {
        vm.stack.push(vm.stack.pop<utl::Isize>() + 1);
    }


    auto jump(VM& vm) -> void {
        vm.jump_to(vm.extract_argument<vm::Jump_offset_type>());
    }

    template <bool value>
    auto jump_bool(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Jump_offset_type>();
        if (vm.stack.pop<bool>() == value) {
            vm.jump_to(offset);
        }
    }

    auto local_jump(VM& vm) -> void {
        vm.instruction_pointer += vm.extract_argument<vm::Local_offset_type>();
    }

    template <bool value>
    auto local_jump_bool(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Local_offset_type>();
        if (vm.stack.pop<bool>() == value) {
            vm.instruction_pointer += offset;
        }
    }


    template <class T, template <class> class F>
        requires std::is_same_v<std::invoke_result_t<F<T>, T, T>, bool>
    auto local_jump_immediate(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Local_offset_type>();
        auto const right  = vm.stack.pop<T>();
        auto const left   = vm.extract_argument<T>();

        if (F<T>{}(left, right)) {
            vm.instruction_pointer += offset;
        }
    }

    template <class T> constexpr auto local_jump_eq_i  = local_jump_immediate<T, std::equal_to>;
    template <class T> constexpr auto local_jump_neq_i = local_jump_immediate<T, std::not_equal_to>;
    template <class T> constexpr auto local_jump_lt_i  = local_jump_immediate<T, std::less>;
    template <class T> constexpr auto local_jump_lte_i = local_jump_immediate<T, std::less_equal>;
    template <class T> constexpr auto local_jump_gt_i  = local_jump_immediate<T, std::greater>;
    template <class T> constexpr auto local_jump_gte_i = local_jump_immediate<T, std::greater_equal>;


    auto bitcopy_from_stack(VM& vm) -> void {
        auto  const size = vm.extract_argument<vm::Local_size_type>();
        auto* const destination = vm.stack.pop<std::byte*>();

        std::memcpy(destination, vm.stack.pointer -= size, size);
    }

    auto bitcopy_to_stack(VM& vm) -> void {
        auto  const size = vm.extract_argument<vm::Local_size_type>();
        auto* const source = vm.stack.pop<std::byte*>();

        std::memcpy(vm.stack.pointer, source, size);
        vm.stack.pointer += size;
    }


    auto push_address(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Local_offset_type>();
        vm.stack.push(vm.activation_record->pointer() + offset);
    }

    auto push_return_value(VM& vm) -> void {
        vm.stack.push(vm.activation_record->return_value_address);
    }


    auto call(VM& vm) -> void {
        auto  const return_value_size     = vm.extract_argument<vm::Local_size_type>();
        auto* const return_value_address  = vm.stack.pointer;
        auto* const old_activation_record = vm.activation_record;

        vm.stack.pointer += return_value_size; // Reserve space for the return value
        vm.activation_record = reinterpret_cast<vm::Activation_record*>(vm.stack.pointer);

        vm.stack.push(
            vm::Activation_record {
                .return_value_address = return_value_address,
                .return_address       = vm.instruction_pointer + sizeof(vm::Jump_offset_type),
                .caller               = old_activation_record,
            });
        vm.jump_to(vm.extract_argument<vm::Jump_offset_type>());
    }

    auto call_0(VM& vm) -> void {
        auto const old_activation_record = vm.activation_record;
        vm.activation_record = reinterpret_cast<vm::Activation_record*>(vm.stack.pointer);

        vm.stack.push(
            vm::Activation_record {
                .return_address = vm.instruction_pointer + sizeof(vm::Jump_offset_type),
                .caller         = old_activation_record,
            });
        vm.jump_to(vm.extract_argument<vm::Jump_offset_type>());
    }

    auto ret(VM& vm) -> void {
        auto* const ar = vm.activation_record;
        vm.stack.pointer       = ar->pointer();      // pop callee's activation record
        vm.activation_record   = ar->caller;         // restore caller state
        vm.instruction_pointer = ar->return_address; // return control to caller
    }


    auto halt(VM& vm) -> void {
        vm.keep_running = false;
    }


    constexpr auto instructions = std::to_array({
        push <utl::Isize>, push <utl::Float>, push <utl::Char>, push <String>, push_bool<true>, push_bool<false>,
        dup  <utl::Isize>, dup  <utl::Float>, dup  <utl::Char>, dup  <String>, dup  <bool>,
        print<utl::Isize>, print<utl::Float>, print<utl::Char>, print<String>, print<bool>,

        add<utl::Isize>, add<utl::Float>,
        sub<utl::Isize>, sub<utl::Float>,
        mul<utl::Isize>, mul<utl::Float>,
        div<utl::Isize>, div<utl::Float>,

        iinc_top,

        eq <utl::Isize>, eq <utl::Float>, eq <utl::Char>, eq <bool>,
        neq<utl::Isize>, neq<utl::Float>, neq<utl::Char>, neq<bool>,
        lt <utl::Isize>, lt <utl::Float>,
        lte<utl::Isize>, lte<utl::Float>,
        gt <utl::Isize>, gt <utl::Float>,
        gte<utl::Isize>, gte<utl::Float>,

        eq_i <utl::Isize>, eq_i <utl::Float>, eq_i <utl::Char>, eq_i <bool>,
        neq_i<utl::Isize>, neq_i<utl::Float>, neq_i<utl::Char>, neq_i<bool>,
        lt_i <utl::Isize>, lt_i <utl::Float>,
        lte_i<utl::Isize>, lte_i<utl::Float>,
        gt_i <utl::Isize>, gt_i <utl::Float>,
        gte_i<utl::Isize>, gte_i<utl::Float>,

        land, lnand, lor, lnor, lnot,

        cast<utl::Isize, utl::Float>, cast<utl::Float, utl::Isize>,
        cast<utl::Isize, utl::Char >, cast<utl::Char , utl::Isize>,
        cast<utl::Isize, bool>, cast<bool, utl::Isize>,
        cast<utl::Float, bool>,
        cast<utl::Char , bool>,

        bitcopy_from_stack,
        bitcopy_to_stack,
        push_address,
        push_return_value,

        jump            , local_jump,
        jump_bool<true> , local_jump_bool<true>,
        jump_bool<false>, local_jump_bool<false>,

        local_jump_eq_i <utl::Isize>, local_jump_eq_i <utl::Float>, local_jump_eq_i <utl::Char>, local_jump_eq_i <bool>,
        local_jump_neq_i<utl::Isize>, local_jump_neq_i<utl::Float>, local_jump_neq_i<utl::Char>, local_jump_neq_i<bool>,
        local_jump_lt_i <utl::Isize>, local_jump_lt_i <utl::Float>,
        local_jump_lte_i<utl::Isize>, local_jump_lte_i<utl::Float>,
        local_jump_gt_i <utl::Isize>, local_jump_gt_i <utl::Float>,
        local_jump_gte_i<utl::Isize>, local_jump_gte_i<utl::Float>,

        call, call_0, ret,

        halt
    });

    static_assert(instructions.size() == utl::enumerator_count<vm::Opcode>);

}


auto vm::Virtual_machine::run() -> int {
    instruction_pointer = program.bytecode.bytes.data();
    instruction_anchor = instruction_pointer;
    keep_running = true;

    // The first activation record does not need to be initialized

    if (program.constants.string_pool.empty()) {
        // move this somewhere else

        program.constants.string_pool.reserve(program.constants.string_buffer_views.size());

        for (auto const [offset, length] : program.constants.string_buffer_views)
            program.constants.string_pool.emplace_back(program.constants.string_buffer.data() + offset, length);

        utl::release_vector_memory(program.constants.string_buffer_views);
    }

    while (keep_running) {
        auto const opcode = extract_argument<Opcode>();
        //fmt::print(" -> {}\n", opcode);
        instructions[utl::as_index(opcode)](*this);
    }

    flush_output();

    return static_cast<int>(stack.pop<utl::Isize>());
}


auto vm::Virtual_machine::jump_to(Jump_offset_type const offset) noexcept -> void {
    instruction_pointer = instruction_anchor + offset;
}


template <utl::trivial T>
auto vm::Virtual_machine::extract_argument() noexcept -> T {
    if constexpr (sizeof(T) == 1) {
        return static_cast<T>(*instruction_pointer++);
    }
    else {
        T argument;
        std::memcpy(&argument, instruction_pointer, sizeof(T));
        instruction_pointer += sizeof(T);
        return argument;
    }
}


auto vm::Virtual_machine::flush_output() -> void {
    std::cout << output_buffer;
    output_buffer.clear();
}


auto vm::argument_bytes(Opcode const opcode) noexcept -> utl::Usize {
    static constexpr auto bytecounts = std::to_array<utl::Usize>({
        sizeof(utl::Isize), sizeof(utl::Float), sizeof(utl::Char), sizeof(utl::Usize), 0, 0, // push

        0, 0, 0, 0, 0, // dup
        0, 0, 0, 0, 0, // print

        0, 0, // add
        0, 0, // sub
        0, 0, // mul
        0, 0, // div

        0, // iinc_top

        0, 0, 0, 0, // eq
        0, 0, 0, 0, // neq
        0, 0,       // lt
        0, 0,       // lte
        0, 0,       // gt
        0, 0,       // gte

        sizeof(utl::Isize), sizeof(utl::Float), sizeof(utl::Char), 1, // eq_i
        sizeof(utl::Isize), sizeof(utl::Float), sizeof(utl::Char), 1, // neq_i
        sizeof(utl::Isize), sizeof(utl::Float),                      // lt_i
        sizeof(utl::Isize), sizeof(utl::Float),                      // lte_i
        sizeof(utl::Isize), sizeof(utl::Float),                      // gt_i
        sizeof(utl::Isize), sizeof(utl::Float),                      // gte_i

        0, 0, 0, 0, 0, // logic

        0, 0, // itof, ftoi
        0, 0, // itoc, ctoi
        0, 0, // itob, btoi
        0,    // ftob
        0,    // ctob

        sizeof(Local_size_type),   // bitcopy_from
        sizeof(Local_size_type),   // bitcopy_to
        sizeof(Local_offset_type), // push_address
        0,                         // push_return_value_address

        sizeof(Jump_offset_type), sizeof(Jump_offset_type), sizeof(Jump_offset_type),    // jump
        sizeof(Local_offset_type), sizeof(Local_offset_type), sizeof(Local_offset_type), // local_jump

        sizeof(Local_offset_type) + sizeof(utl::Isize), sizeof(Local_offset_type) + sizeof(utl::Float), sizeof(Local_offset_type) + sizeof(utl::Char), sizeof(Local_offset_type) + 1, // local_jump_eq
        sizeof(Local_offset_type) + sizeof(utl::Isize), sizeof(Local_offset_type) + sizeof(utl::Float), sizeof(Local_offset_type) + sizeof(utl::Char), sizeof(Local_offset_type) + 1, // local_jump_neq
        sizeof(Local_offset_type) + sizeof(utl::Isize), sizeof(Local_offset_type) + sizeof(utl::Float), // local_jump_lt
        sizeof(Local_offset_type) + sizeof(utl::Isize), sizeof(Local_offset_type) + sizeof(utl::Float), // local_jump_lte
        sizeof(Local_offset_type) + sizeof(utl::Isize), sizeof(Local_offset_type) + sizeof(utl::Float), // local_jump_gt
        sizeof(Local_offset_type) + sizeof(utl::Isize), sizeof(Local_offset_type) + sizeof(utl::Float), // local_jump_gte

        sizeof(Local_size_type) + sizeof(Jump_offset_type), // call
        sizeof(Jump_offset_type),                           // call_0
        0,                                                  // ret

        0, // halt
    });
    static_assert(bytecounts.size() == utl::enumerator_count<vm::Opcode>);
    return bytecounts[utl::as_index(opcode)];
}
