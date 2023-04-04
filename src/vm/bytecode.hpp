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

        template <utl::trivial... Args>
        struct [[nodiscard]] Reserved_slots_for {
            Bytecode*  bytecode;
            utl::Usize offset;

            auto write_to_reserved(Args const... args) const noexcept -> void {
                utl::serialize_to(bytecode->bytes.data() + offset, args...);
            }
        };

        template <utl::trivial... Args>
        auto reserve_slots_for() -> Reserved_slots_for<Args...> {
            Reserved_slots_for<Args...> const rs { .bytecode = this, .offset = current_offset() };
            bytes.insert(bytes.end(), (sizeof(Args) + ...), std::byte {});
            return rs;
        }
    };

}