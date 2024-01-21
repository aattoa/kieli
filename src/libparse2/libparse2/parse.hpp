#pragma once

#include <libphase/phase.hpp>
#include <libparse2/cst.hpp>

namespace kieli {
    auto parse2(utl::Source::Wrapper source, Compile_info& compile_info) -> cst::Module;
}
