#pragma once

#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli {
    auto parse(Source_id source, Database& db) -> CST;
}
