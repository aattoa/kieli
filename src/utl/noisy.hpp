#pragma once

#include "utl/utilities.hpp"


namespace utl {

#define BU_NOISY_LOG(op, f, ...) \
f noexcept { op live_count; fmt::print("[{},{}] utl::Noisy::" #f "\n", live_count, static_cast<void const*>(this)); __VA_ARGS__ }

    struct Noisy {
        inline static constinit Usize live_count = 0;
        BU_NOISY_LOG(++, Noisy());
        BU_NOISY_LOG(++, Noisy(Noisy const&));
        BU_NOISY_LOG(++, Noisy(Noisy&&));
        BU_NOISY_LOG(--, ~Noisy());
        Noisy& BU_NOISY_LOG((void), operator=(Noisy const&), return *this;);
        Noisy& BU_NOISY_LOG((void), operator=(Noisy&&), return *this;);
    };

#undef BU_NOISY_LOG

}
