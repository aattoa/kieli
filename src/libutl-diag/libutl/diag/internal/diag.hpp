#pragma once

#include <libutl/diag/diag.hpp>

namespace utl::diag::internal {
    [[nodiscard]] auto get_relevant_lines(
        std::string_view source_string, Position section_start, Position section_stop)
        -> std::vector<std::string_view>;
}
