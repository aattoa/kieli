#include "utl/utilities.hpp"
#include "virtual_machine.hpp"
#include "vm_formatting.hpp"


namespace {

    constexpr auto opcode_strings = std::to_array<std::string_view>({
        "halt", "halt_with",

        "const_1", "const_2", "const_4", "const_8", "const_string", "const_true", "const_false",
        "dup_1", "dup_2", "dup_4", "dup_8", "dup_n",

        "iprint", "fprint", "cprint", "sprint", "bprint",

        "pop_1", "pop_2", "pop_4", "pop_8", "pop_n",

        "iadd", "fadd",
        "isub", "fsub",
        "imul", "fmul",
        "idiv", "fdiv",

        "iinc_top",

        "ieq" , "feq" , "ceq" , "beq" ,
        "ineq", "fneq", "cneq", "bneq",
        "ilt" , "flt" ,
        "ilte", "flte",
        "igt" , "fgt" ,
        "igte", "fgte",

        "ieq_i" , "feq_i" , "ceq_i" , "beq_i" ,
        "ineq_i", "fneq_i", "cneq_i", "bneq_i",
        "ilt_i" , "flt_i" ,
        "ilte_i", "flte_i",
        "igt_i" , "fgt_i" ,
        "igte_i", "fgte_i",

        "land", "lnand", "lor", "lnor", "lnot",

        "cast_itof", "cast_ftoi",
        "cast_itoc", "cast_ctoi",

        "cast_itob", "cast_btoi",
        "cast_ftob",
        "cast_ctob",

        "reserve_stack_space",
        "bitcopy_from_stack",
        "bitcopy_to_stack",
        "bitcopy_from_local",
        "bitcopy_to_local",
        "push_local_address",
        "push_return_value_address",

        "jump",       "local_jump",
        "jump_true",  "local_jump_true",
        "jump_false", "local_jump_false",

        "local_jump_ieq_i" , "local_jump_feq_i" , "local_jump_ceq_i" , "local_jump_beq_i" ,
        "local_jump_ineq_i", "local_jump_fneq_i", "local_jump_cneq_i", "local_jump_bneq_i",
        "local_jump_ilt_i" , "local_jump_flt_i" ,
        "local_jump_ilte_i", "local_jump_flte_i",
        "local_jump_igt_i" , "local_jump_fgt_i" ,
        "local_jump_igte_i", "local_jump_fgte_i",

        "call", "call_0", "call_ptr", "call_ptr_0", "ret",
    });
    static_assert(opcode_strings.size() == utl::enumerator_count<vm::Opcode>);


    template <class T>
    auto extract(std::byte const*& start, std::byte const* stop) noexcept -> T {
        assert(start + sizeof(T) <= stop);
        (void)stop;
        T x;
        std::memcpy(&x, start, sizeof x);
        start += sizeof x;
        return x;
    }

    auto format_instruction(
        fmt::format_context::iterator out,
        std::byte const*&             start,
        std::byte const*              stop)
    {
        auto const opcode = extract<vm::Opcode>(start, stop);

        auto const unary = [&]<class T>(utl::Type<T>) {
            return fmt::format_to(out, "{} {}", opcode, extract<T>(start, stop));
        };

        auto const binary = [&]<class T, class U>(utl::Type<T>, utl::Type<U>) {
            auto const first = extract<T>(start, stop);
            auto const second = extract<U>(start, stop);
            return fmt::format_to(out, "{} {} {}", opcode, first, second);
        };

        auto const constant = [&]<class T>(utl::Type<T>) {
            return fmt::format_to(out, "{} {:#x}", opcode, extract<T>(start, stop));
        };

        switch (opcode) {
        case vm::Opcode::const_1:
            return constant(utl::type<utl::U8>);
        case vm::Opcode::const_2:
            return constant(utl::type<utl::U16>);
        case vm::Opcode::const_4:
            return constant(utl::type<utl::U32>);
        case vm::Opcode::const_8:
            return constant(utl::type<utl::U64>);
        case vm::Opcode::const_string:
            return constant(utl::type<utl::Usize>);

        case vm::Opcode::ieq_i:
        case vm::Opcode::ineq_i:
        case vm::Opcode::ilt_i:
        case vm::Opcode::ilte_i:
        case vm::Opcode::igt_i:
        case vm::Opcode::igte_i:
            return unary(utl::type<utl::Isize>);

        case vm::Opcode::feq_i:
        case vm::Opcode::fneq_i:
        case vm::Opcode::flt_i:
        case vm::Opcode::flte_i:
        case vm::Opcode::fgt_i:
        case vm::Opcode::fgte_i:
            return unary(utl::type<utl::Float>);

        case vm::Opcode::ceq_i:
        case vm::Opcode::cneq_i:
            return unary(utl::type<utl::Char>);

        case vm::Opcode::beq_i:
        case vm::Opcode::bneq_i:
            return unary(utl::type<bool>);

        case vm::Opcode::call_ptr:
        case vm::Opcode::pop_n:
        case vm::Opcode::dup_n:
        case vm::Opcode::reserve_stack_space:
        case vm::Opcode::bitcopy_from_stack:
        case vm::Opcode::bitcopy_to_stack:
            return unary(utl::type<vm::Local_size_type>);

        case vm::Opcode::bitcopy_from_local:
        case vm::Opcode::bitcopy_to_local:
            return binary(utl::type<vm::Local_size_type>, utl::type<vm::Local_offset_type>);

        case vm::Opcode::jump:
        case vm::Opcode::jump_true:
        case vm::Opcode::jump_false:
        case vm::Opcode::call_0:
            return unary(utl::type<vm::Jump_offset_type>);

        case vm::Opcode::push_local_address:
        case vm::Opcode::local_jump:
        case vm::Opcode::local_jump_true:
        case vm::Opcode::local_jump_false:
            return unary(utl::type<vm::Local_offset_type>);

        case vm::Opcode::local_jump_ieq_i:
        case vm::Opcode::local_jump_ineq_i:
        case vm::Opcode::local_jump_ilt_i:
        case vm::Opcode::local_jump_ilte_i:
        case vm::Opcode::local_jump_igt_i:
        case vm::Opcode::local_jump_igte_i:
            return binary(utl::type<vm::Local_offset_type>, utl::type<utl::Isize>);

        case vm::Opcode::local_jump_feq_i:
        case vm::Opcode::local_jump_fneq_i:
        case vm::Opcode::local_jump_flt_i:
        case vm::Opcode::local_jump_flte_i:
        case vm::Opcode::local_jump_fgt_i:
        case vm::Opcode::local_jump_fgte_i:
            return binary(utl::type<vm::Local_offset_type>, utl::type<utl::Float>);

        case vm::Opcode::local_jump_ceq_i:
        case vm::Opcode::local_jump_cneq_i:
            return binary(utl::type<vm::Local_offset_type>, utl::type<utl::Char>);

        case vm::Opcode::local_jump_beq_i:
        case vm::Opcode::local_jump_bneq_i:
            return binary(utl::type<vm::Local_offset_type>, utl::type<bool>);

        case vm::Opcode::call:
            return binary(utl::type<vm::Local_size_type>, utl::type<vm::Jump_offset_type>);

        default:
            assert(vm::argument_bytes(opcode) == 0);
            return fmt::format_to(out, "{}", opcode);
        }
    }

}


DEFINE_FORMATTER_FOR(vm::Opcode) {
    return fmt::format_to(context.out(), "{}", opcode_strings[utl::as_index(value)]);
}

DEFINE_FORMATTER_FOR(vm::Bytecode) {
    auto const* const start = value.bytes.data();
    auto const* const stop  = start + value.bytes.size();

    auto out = context.out();

    auto const digit_count = utl::digit_count(utl::unsigned_distance(start, stop));

    for (auto const* pointer = start; pointer < stop; ) {
        out = fmt::format_to(out, "{:>{}} ", std::distance(start, pointer), digit_count);
        out = format_instruction(out, pointer, stop);
        out = fmt::format_to(out, "\n");
    }

    return out;
}