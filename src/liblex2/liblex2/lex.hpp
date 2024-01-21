#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>
#include <liblex2/token.hpp>

namespace kieli {
    struct Lex2_state {
        Compile_info&        compile_info;
        utl::Source::Wrapper source;
        utl::Source_position position;
        std::string_view     string;

        static auto make(utl::Source::Wrapper, Compile_info&) -> Lex2_state;
    };

    auto lex2(Lex2_state&) -> Token2;
} // namespace kieli
