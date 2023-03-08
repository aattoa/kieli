#pragma once

#include "utl/utilities.hpp"
#include "utl/color.hpp"


namespace utl {

    template <class Clock_type, class Duration_type>
    struct Basic_timer {
        using Clock    = Clock_type;
        using Duration = Duration_type;

        Clock::time_point start = Clock::now();

        auto restart(Clock::time_point const new_start = Clock::now()) noexcept -> void {
            start = new_start;
        }
        auto elapsed() const noexcept -> Duration {
            return std::chrono::duration_cast<Duration>(Clock::now() - start);
        }
    };

    template <class Clock_type, class Duration_type>
    struct Basic_logging_timer : Basic_timer<Clock_type, Duration_type> {
        using Clock    = Clock_type;
        using Duration = Duration_type;

        std::function<void(Duration)> scope_exit_logger;

        Basic_logging_timer() noexcept
            : scope_exit_logger
        {
            [](Duration const duration) {
                print(
                    "[{}utl::Logging_timer::~Logging_timer{}]: Total elapsed time: {}\n",
                    Color::purple,
                    Color::white,
                    duration
                );
            }
        } {}

        explicit Basic_logging_timer(decltype(scope_exit_logger)&& scope_exit_logger) noexcept
            : scope_exit_logger { std::move(scope_exit_logger) } {}

        ~Basic_logging_timer() {
            if (scope_exit_logger) {
                scope_exit_logger(Basic_timer<Clock, Duration>::elapsed());
            }
            else {
                abort("utl::Logging_timer: scope_exit_logger was uninitialized");
            }
        }
    };

    using Timer         = Basic_timer        <std::chrono::steady_clock, std::chrono::milliseconds>;
    using Logging_timer = Basic_logging_timer<std::chrono::steady_clock, std::chrono::milliseconds>;

}