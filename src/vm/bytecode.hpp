#pragma once

#include "utl/utilities.hpp"


namespace vm {

    struct Bytecode {
        std::vector<std::byte> bytes;

        auto write(utl::trivial auto const... args) noexcept -> void {
            utl::serialize_to(std::back_inserter(bytes), args...);
        }
        [[nodiscard]]
        auto current_offset() const noexcept -> utl::Usize {
            return bytes.size();
        }
    };

}