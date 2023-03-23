#pragma once

#include "utl/utilities.hpp"
#include "utl/bytestack.hpp"
#include "bytecode.hpp"


namespace vm {

    using Jump_offset_type  = utl::Usize;
    using Local_offset_type = utl::I16; // signed because function parameters use negative offsets
    using Local_size_type   = std::make_unsigned_t<Local_offset_type>;


    struct Activation_record {
        std::byte*         return_value_address;
        std::byte*         return_address;
        Activation_record* caller;

        auto pointer() noexcept -> std::byte* {
            return reinterpret_cast<std::byte*>(this);
        }
    };


    struct Constants {
        struct String {
            char const* pointer;
            utl::Usize   length;
        };

        std::string                      string_buffer;
        std::vector<utl::Pair<utl::Usize>> string_buffer_views;
        std::vector<String>              string_pool;

        auto add_to_string_pool(std::string_view const string) noexcept -> utl::Usize {
            auto const offset = string_buffer.size();
            string_buffer.append(string);
            string_buffer_views.emplace_back(offset, string.size());
            return string_buffer_views.size() - 1;
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
        utl::Bytestack      stack;
        std::byte*         instruction_pointer = nullptr;
        std::byte*         instruction_anchor  = nullptr;
        Activation_record* activation_record   = nullptr;
        bool               keep_running        = true;


        auto run() -> int;

        auto jump_to(Jump_offset_type) noexcept -> void;

        template <utl::trivial T>
        auto extract_argument() noexcept -> T;


        std::string output_buffer;

        auto flush_output() -> void;
    };

}