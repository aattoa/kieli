#pragma once

#include "utl/utilities.hpp"


namespace vm {

    enum class Opcode : utl::U8 {
        ipush , fpush , cpush , spush , push_true, push_false,
        idup  , fdup  , cdup  , sdup  , bdup  ,
        iprint, fprint, cprint, sprint, bprint,

        pop_1, pop_2, pop_4, pop_8, pop_n,

        iadd, fadd,
        isub, fsub,
        imul, fmul,
        idiv, fdiv,

        iinc_top,

        ieq , feq , ceq , beq ,
        ineq, fneq, cneq, bneq,
        ilt , flt ,
        ilte, flte,
        igt , fgt ,
        igte, fgte,

        ieq_i , feq_i , ceq_i , beq_i ,
        ineq_i, fneq_i, cneq_i, bneq_i,
        ilt_i , flt_i ,
        ilte_i, flte_i,
        igt_i , fgt_i ,
        igte_i, fgte_i,

        land, lnand, lor, lnor, lnot,

        cast_itof, cast_ftoi,
        cast_itoc, cast_ctoi,
        cast_itob, cast_btoi,
        cast_ftob,
        cast_ctob,

        bitcopy_from_stack,
        bitcopy_to_stack,
        push_address,
        push_return_value_address,

        jump,       local_jump,
        jump_true,  local_jump_true,
        jump_false, local_jump_false,

        local_jump_ieq_i , local_jump_feq_i , local_jump_ceq_i , local_jump_beq_i ,
        local_jump_ineq_i, local_jump_fneq_i, local_jump_cneq_i, local_jump_bneq_i,
        local_jump_ilt_i , local_jump_flt_i ,
        local_jump_ilte_i, local_jump_flte_i,
        local_jump_igt_i , local_jump_fgt_i ,
        local_jump_igte_i, local_jump_fgte_i,

        call, call_0, ret,

        halt,

        _enumerator_count
    };

    auto argument_bytes(Opcode) noexcept -> utl::Usize;

}