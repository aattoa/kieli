#pragma once

#include "utl/utilities.hpp"


namespace utl {
    [[nodiscard]]
    auto readline(char const* prompt) -> std::string;
}