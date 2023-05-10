#include <libutl/common/utilities.hpp>
#include <libvm/virtual_machine.hpp>
#include <libvm/opcode.hpp>


namespace {

    using VM     = vm::Virtual_machine;
    using String = std::string_view;


    auto current_activation_record(VM& vm) noexcept -> vm::Activation_record {
        vm::Activation_record ar; // NOLINT
        std::memcpy(&ar, vm.activation_record, sizeof ar);
        return ar;
    }


    template <utl::Usize n>
    auto const_(VM& vm) -> void {
        std::memcpy(vm.stack.pointer, vm.instruction_pointer, n);
        vm.stack.pointer       += n;
        vm.instruction_pointer += n;
    }

    auto const_string(VM& vm) -> void {
        vm.stack.push(vm.program.constants.strings[vm.extract_argument<utl::Usize>()]);
    }

    template <bool value>
    auto const_bool(VM& vm) -> void {
        vm.stack.push(value);
    }

    template <utl::Usize n>
    auto dup(VM& vm) -> void {
        std::memcpy(vm.stack.pointer, vm.stack.pointer - n, n);
        vm.stack.pointer += n;
    }

    auto dup_n(VM& vm) -> void {
        auto const n = vm.extract_argument<vm::Local_size_type>();
        std::memcpy(vm.stack.pointer, vm.stack.pointer - n, n);
        vm.stack.pointer += n;
    }

    template <utl::Usize n>
    auto pop(VM& vm) -> void {
        vm.stack.pointer -= n;
    }

    auto pop_n(VM& vm) -> void {
        vm.stack.pointer -= vm.extract_argument<vm::Local_size_type>();
    }

    template <utl::trivially_copyable T>
    auto print(VM& vm) -> void {
        auto const popped = vm.stack.pop<T>();

        if constexpr (std::same_as<T, String>)
            vm.output_buffer.append(popped);
        else
            fmt::format_to(std::back_inserter(vm.output_buffer), "{}\n", popped);

        vm.flush_output(); // TODO: adjust flush frequency
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
    template <class T> constexpr auto div_ = binary_op<T, std::divides>;

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
        if (vm.stack.pop<bool>() == value)
            vm.jump_to(offset);
    }

    auto local_jump(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Local_offset_type>();
        vm.instruction_pointer += offset;
    }

    template <bool value>
    auto local_jump_bool(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Local_offset_type>();
        if (vm.stack.pop<bool>() == value)
            vm.instruction_pointer += offset;
    }


    template <class T, template <class> class F>
        requires std::is_same_v<std::invoke_result_t<F<T>, T, T>, bool>
    auto local_jump_immediate(VM& vm) -> void {
        auto const offset = vm.extract_argument<vm::Local_offset_type>();
        auto const right  = vm.stack.pop<T>();
        auto const left   = vm.extract_argument<T>();

        if (F<T>{}(left, right))
            vm.instruction_pointer += offset;
    }

    template <class T> constexpr auto local_jump_eq_i  = local_jump_immediate<T, std::equal_to>;
    template <class T> constexpr auto local_jump_neq_i = local_jump_immediate<T, std::not_equal_to>;
    template <class T> constexpr auto local_jump_lt_i  = local_jump_immediate<T, std::less>;
    template <class T> constexpr auto local_jump_lte_i = local_jump_immediate<T, std::less_equal>;
    template <class T> constexpr auto local_jump_gt_i  = local_jump_immediate<T, std::greater>;
    template <class T> constexpr auto local_jump_gte_i = local_jump_immediate<T, std::greater_equal>;


    auto reserve_stack_space(VM& vm) -> void {
        vm.stack.pointer += vm.extract_argument<vm::Local_size_type>();
    }

    auto bitcopy_from_stack(VM& vm) -> void {
        auto  const size = vm.extract_argument<vm::Local_size_type>();
        auto* const destination = vm.stack.pop<std::byte*>();
        std::memcpy(destination, vm.stack.pointer -= size, size);
    }

    auto bitcopy_to_stack(VM& vm) -> void {
        auto  const size   = vm.extract_argument<vm::Local_size_type>();
        auto* const source = vm.stack.pop<std::byte*>();
        std::memcpy(vm.stack.pointer, source, size);
        vm.stack.pointer += size;
    }

    auto bitcopy_from_local(VM& vm) -> void {
        auto  const size  = vm.extract_argument<vm::Local_size_type>();
        auto* const local = vm.activation_record + vm.extract_argument<vm::Local_offset_type>();
        std::memcpy(vm.stack.pointer, local, size);
        vm.stack.pointer += size;
    }

    auto bitcopy_to_local(VM& vm) -> void {
        auto  const size  = vm.extract_argument<vm::Local_size_type>();
        auto* const local = vm.activation_record + vm.extract_argument<vm::Local_offset_type>();
        std::memcpy(local, vm.stack.pointer -= size, size);
    }


    auto push_local_address(VM& vm) -> void {
        vm.stack.push(vm.activation_record + vm.extract_argument<vm::Local_offset_type>());
    }

    auto push_return_value_address(VM& vm) -> void {
        vm.stack.push(current_activation_record(vm).return_value_address);
    }


    template <bool is_indirect>
    auto call(VM& vm) -> void {
        std::byte* const return_value_address = vm.stack.pointer;
        std::byte* const old_activation_record = vm.activation_record;

        // Reserve stack space for the return value
        vm.stack.pointer += vm.extract_argument<vm::Local_size_type>();

        // Where control will be returned by the corresponding ret instruction
        std::byte* const return_address =
            vm.instruction_pointer + (is_indirect ? 0 : sizeof(vm::Jump_offset_type));

        // Activate the callee
        vm.activation_record = vm.stack.pointer;
        vm.stack.push(vm::Activation_record {
            .return_value_address     = return_value_address,
            .return_address           = return_address,
            .caller_activation_record = old_activation_record,
        });

        // Give control to the callee
        if constexpr (is_indirect)
            vm.instruction_pointer = vm.stack.pop<std::byte*>();
        else
            vm.jump_to(vm.extract_argument<vm::Jump_offset_type>());
    }

    template <bool is_indirect>
    auto call_0(VM& vm) -> void {
        std::byte* const old_activation_record = vm.activation_record;

        // Where control will be returned by the corresponding ret instruction
        std::byte* const return_address =
            vm.instruction_pointer + (is_indirect ? 0 : sizeof(vm::Jump_offset_type));

        // Activate the callee
        vm.activation_record = vm.stack.pointer;
        vm.stack.push(vm::Activation_record {
            .return_address           = return_address,
            .caller_activation_record = old_activation_record,
        });

        // Give control to the callee
        if constexpr (is_indirect)
            vm.instruction_pointer = vm.stack.pop<std::byte*>();
        else
            vm.jump_to(vm.extract_argument<vm::Jump_offset_type>());
    }

    auto ret(VM& vm) -> void {
        vm::Activation_record const ar = current_activation_record(vm);
        vm.stack.pointer       = vm.activation_record;        // pop callee's activation record
        vm.activation_record   = ar.caller_activation_record; // restore caller state
        vm.instruction_pointer = ar.return_address;           // return control to caller
    }


    auto halt(VM& vm) -> void {
        vm.keep_running = false;
    }
    auto halt_with(VM& vm) -> void {
        vm.return_value = utl::safe_cast<int>(vm.stack.pop<utl::Isize>());
        vm.keep_running = false;
    }


    constexpr auto instructions = std::to_array({
        halt, halt_with,

        const_<1>, const_<2>, const_<4>, const_<8>, const_string, const_bool<true>, const_bool<false>,

        dup<1>, dup<2>, dup<4>, dup<8>, dup_n,

        print<utl::Isize>, print<utl::Float>, print<utl::Char>, print<String>, print<bool>,

        pop<1>, pop<2>, pop<4>, pop<8>, pop_n,

        add<utl::Isize>, add<utl::Float>,
        sub<utl::Isize>, sub<utl::Float>,
        mul<utl::Isize>, mul<utl::Float>,
        div_<utl::Isize>, div_<utl::Float>,

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

        reserve_stack_space,
        bitcopy_from_stack,
        bitcopy_to_stack,
        bitcopy_from_local,
        bitcopy_to_local,
        push_local_address,
        push_return_value_address,

        jump            , local_jump,
        jump_bool<true> , local_jump_bool<true>,
        jump_bool<false>, local_jump_bool<false>,

        local_jump_eq_i <utl::Isize>, local_jump_eq_i <utl::Float>, local_jump_eq_i <utl::Char>, local_jump_eq_i <bool>,
        local_jump_neq_i<utl::Isize>, local_jump_neq_i<utl::Float>, local_jump_neq_i<utl::Char>, local_jump_neq_i<bool>,
        local_jump_lt_i <utl::Isize>, local_jump_lt_i <utl::Float>,
        local_jump_lte_i<utl::Isize>, local_jump_lte_i<utl::Float>,
        local_jump_gt_i <utl::Isize>, local_jump_gt_i <utl::Float>,
        local_jump_gte_i<utl::Isize>, local_jump_gte_i<utl::Float>,

        call<false>, call_0<false>, call<true>, call_0<true>, ret,
    });

    static_assert(instructions.size() == utl::enumerator_count<vm::Opcode>);

}


auto vm::Virtual_machine::run() -> int {
    instruction_pointer = program.bytecode.bytes.data();
    instruction_anchor = instruction_pointer;
    keep_running = true;

    // The first activation record does not need to be initialized

    while (keep_running) {
        auto const opcode = extract_argument<Opcode>();
        // fmt::println("executing {}...", opcode);
        instructions[utl::as_index(opcode)](*this);
    }

    flush_output();

    return return_value;
}


auto vm::Virtual_machine::jump_to(Jump_offset_type const offset) noexcept -> void {
    instruction_pointer = instruction_anchor + offset;
}


template <utl::trivially_copyable T>
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


