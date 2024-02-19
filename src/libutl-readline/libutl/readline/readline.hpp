#pragma once

#include <libutl/common/utilities.hpp>

namespace utl {

    [[nodiscard]] auto readline(char const* prompt) -> std::optional<std::string>;

    auto add_to_readline_history(std::string const&) -> void;

} // namespace utl
