#include <libutl/common/utilities.hpp>
#include <libvm/virtual_machine.hpp>
#include <libvm/opcode.hpp>
#include <catch2/catch_test_macros.hpp>


namespace {
    auto run_bytecode(utl::trivial auto const... program) -> int {
        vm::Virtual_machine machine { .stack = utl::Bytestack { 256 } };
        machine.program.bytecode.write(program...);
        return machine.run();
    }
}


TEST_CASE("arithmetic", "[VM]") {
    REQUIRE(10 == run_bytecode(
        vm::Opcode::const_8, 2_iz,
        vm::Opcode::const_8, 5_iz,
        vm::Opcode::imul,
        vm::Opcode::halt_with
    ));
    REQUIRE(100 == run_bytecode(
        vm::Opcode::const_8, 2_iz,
        vm::Opcode::const_8, 4_iz,
        vm::Opcode::imul,

        vm::Opcode::const_8, 5_iz,
        vm::Opcode::const_8, 5_iz,
        vm::Opcode::iadd,

        vm::Opcode::imul,

        vm::Opcode::const_8, 10_iz,
        vm::Opcode::const_8, 6_iz,
        vm::Opcode::isub,
        vm::Opcode::const_8, 5_iz,
        vm::Opcode::imul,

        vm::Opcode::iadd,
        vm::Opcode::halt_with
    ));
}
