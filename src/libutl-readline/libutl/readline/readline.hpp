#pragma once

#include <libutl/common/utilities.hpp>


namespace utl {
    [[nodiscard]]
    auto readline(std::string const& prompt) -> std::string;
    auto add_to_readline_history(std::string const&) -> void;

    /* The ideal prompt string type would be std::string_view, but
     * the underlying readline C API expects a null-terminated raw
     * character pointer. This leaves two options: char const* and
     * std::string, out of which std::string is the better option. */
}
