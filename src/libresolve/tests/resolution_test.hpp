#pragma once

#include <libutl/common/utilities.hpp>

namespace libresolve {
    struct Do_test_resolve_result {
        std::string formatted_hir_functions;
        std::string diagnostics_messages;
    };

    auto do_test_resolve(std::string) -> Do_test_resolve_result;
} // namespace libresolve
