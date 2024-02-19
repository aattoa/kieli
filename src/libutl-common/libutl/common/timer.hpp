#pragma once

#include <chrono>

namespace utl {

    template <class Clock_type, class Duration_type>
    class Basic_timer {
        Clock_type::time_point m_start = Clock_type::now();
    public:
        using Clock    = Clock_type;
        using Duration = Duration_type;

        auto restart(Clock::time_point const new_start = Clock::now()) noexcept -> void
        {
            m_start = new_start;
        }

        auto elapsed() const -> Duration
        {
            return std::chrono::duration_cast<Duration>(Clock::now() - m_start);
        }
    };

    using Timer = Basic_timer<std::chrono::steady_clock, std::chrono::milliseconds>;

} // namespace utl
