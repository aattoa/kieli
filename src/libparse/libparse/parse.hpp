#pragma once

#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli {
    auto parse(Source_id source, Compile_info& compile_info) -> CST;
}
