#pragma once

#include "utl/utilities.hpp"
#include "utl/bytestack.hpp"
#include "utl/pooled_string.hpp"
#include "bytecode.hpp"


namespace vm {

    using Jump_offset_type  = utl::Usize;
    using Local_offset_type = utl::I16; // signed because function parameters use negative offsets
    using Local_size_type   = std::make_unsigned_t<Local_offset_type>;


    struct Activation_record {
        std::byte* return_value_address;
        std::byte* return_address;
        std::byte* caller_activation_record;
    };


    struct Constants {
        using String = utl::Pooled_string<struct _vm_string_tag>;
        String::Pool                  string_pool;
        std::vector<std::string_view> strings;

        static_assert(std::is_trivially_copyable_v<std::string_view>);

        [[nodiscard]]
        auto add_to_string_pool(std::string_view const string) -> utl::Usize {
            strings.push_back(string_pool.make(string).view());
            return strings.size() - 1;
        }
    };


    // Represents one compiled module
    struct Compiled_module {
        Bytecode  bytecode;
        Constants constants;

        // add offset handling
    };


    // Represents an entire program, produced by linking one or more compiled modules
    struct Executable_program {
        Bytecode  bytecode;
        Constants constants;
        utl::Usize stack_capacity;

        [[nodiscard]]
        auto serialize() const -> std::vector<std::byte>;

        static auto deserialize(std::span<std::byte const>) -> Executable_program;
    };


    struct [[nodiscard]] Virtual_machine {
        Executable_program program;
        utl::Bytestack     stack;
        std::byte*         instruction_pointer = nullptr;
        std::byte*         instruction_anchor  = nullptr;
        std::byte*         activation_record   = nullptr;
        bool               keep_running        = true;
        int                return_value        = 0;


        auto run() -> int;

        auto jump_to(Jump_offset_type) noexcept -> void;

        template <utl::trivially_copyable T>
        auto extract_argument() noexcept -> T;


        std::string output_buffer;

        auto flush_output() -> void;
    };

}