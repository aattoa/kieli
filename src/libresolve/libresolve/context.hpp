#pragma once

#include <libutl/common/utilities.hpp>
#include <libresolve/module.hpp>
#include <libresolve/hir.hpp>

namespace libresolve {

    struct Context {
        Info_arena  info_arena;
        Environment root_environment;
    };

} // namespace libresolve
