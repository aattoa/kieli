#include "utl/utilities.hpp"
#include "virtual_machine.hpp"
#include "opcode.hpp"


auto vm::argument_bytes(Opcode const opcode) noexcept -> utl::Usize {
    static constexpr auto bytecounts = std::to_array<utl::Usize>({
         0,           // halt
         sizeof(int), // halt_with

         1, 2, 4, 8, sizeof(utl::Usize), 0, 0, // const

         0, 0, 0, 0, sizeof(Local_size_type), // dup

         0, 0, 0, 0, 0, // print

         0, 0, 0, 0, sizeof(Local_size_type), // pop

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

         sizeof(Local_size_type),                             // reserve_stack_space
         sizeof(Local_size_type),                             // bitcopy_from
         sizeof(Local_size_type),                             // bitcopy_to
         sizeof(Local_size_type) + sizeof(Local_offset_type), // bitcopy_from_local
         sizeof(Local_size_type) + sizeof(Local_offset_type), // bitcopy_to_local
         sizeof(Local_offset_type),                           // push_local_address
         0,                                                   // push_return_value_address

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
         sizeof(Local_size_type),                            // call_ptr
         0,                                                  // call_ptr_0
         0,                                                  // ret
     });
    static_assert(bytecounts.size() == utl::enumerator_count<vm::Opcode>);
    return bytecounts[utl::as_index(opcode)];
}
