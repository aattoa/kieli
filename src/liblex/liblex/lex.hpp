#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>
#include <liblex/token.hpp>

namespace kieli {
    auto lex(utl::Source::Wrapper, Compile_info&) -> std::vector<Lexical_token>;
}
