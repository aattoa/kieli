#pragma once

#include <libutl/common/utilities.hpp>
#include <libparse/cst.hpp>
#include <liblex/token.hpp>

namespace kieli {
    auto parse(std::span<Lexical_token const>, Compile_info&) -> cst::Module;
}
