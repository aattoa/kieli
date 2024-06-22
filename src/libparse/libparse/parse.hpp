#pragma once

#include <libcompiler/compiler.hpp>
#include <libparse/cst.hpp>

namespace kieli {
    auto parse(utl::Source_id source, Compile_info& compile_info) -> cst::Module;
}
