#pragma once

#include "utl/utilities.hpp"
#include "phase/lower/lower.hpp"
#include "vm/bytecode.hpp"
#include "vm/virtual_machine.hpp"


namespace compiler {

    struct [[nodiscard]] Function_reference {
        std::string symbol;
        utl::Usize  code_offset {};
    };

    struct [[nodiscard]] Codegen_result {
        vm::Constants::String::Pool        string_pool;
        std::vector<vm::Constants::String> strings;
        vm::Bytecode                       bytecode;
        std::vector<Function_reference>    function_references;
    };

    auto codegen(Lower_result&&) -> Codegen_result;

}