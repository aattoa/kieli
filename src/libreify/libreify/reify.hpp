#pragma once

#include <libphase/phase.hpp>
#include <libresolve/resolve.hpp>
#include <libreify/cir.hpp>

namespace kieli {

    struct [[nodiscard]] Reify_result {
        Compilation_info           compilation_info;
        cir::Node_arena            node_arena;
        std::vector<cir::Function> functions;
    };

    auto reify(Resolve_result&&) -> Reify_result;

} // namespace kieli
