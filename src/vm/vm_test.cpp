#include "utl/utilities.hpp"
#include "tests/tests.hpp"

#include "vm/opcode.hpp"
#include "vm/virtual_machine.hpp"


namespace {

    auto run_bytecode(utl::trivial auto const... program) -> int {
        vm::Virtual_machine machine { .stack = utl::Bytestack { 256 } };
        machine.program.bytecode.write(program...);
        return machine.run();
    }

    auto run_vm_tests() -> void {
        using namespace utl::literals;
        using namespace tests;
        using enum vm::Opcode;

        "arithmetic"_test = [] {
            assert_eq(
                10,
                run_bytecode(
                    const_8, 2_iz,
                    const_8, 5_iz,
                    imul,
                    halt_with));

            assert_eq(
                100,
                run_bytecode(
                    const_8, 2_iz,
                    const_8, 4_iz,
                    imul,

                    const_8, 5_iz,
                    const_8, 5_iz,
                    iadd,

                    imul,

                    const_8, 10_iz,
                    const_8, 6_iz,
                    isub,
                    const_8, 5_iz,
                    imul,

                    iadd,
                    halt_with));
        };
    }

}


REGISTER_TEST(run_vm_tests);