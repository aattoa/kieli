#pragma once

#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>

namespace kieli {
    auto parse(Source_id source, Compile_info& compile_info) -> cst::Module;
}
