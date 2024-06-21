#pragma once

#include <libphase/phase.hpp>
#include <libparse/cst.hpp>

namespace kieli {
    auto parse(utl::Source_id source, Compile_info& compile_info) -> cst::Module;
}
