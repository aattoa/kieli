#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>
#include <liblex/token.hpp>

namespace kieli {

    struct Lex_state {
        Compile_info&        compile_info;
        utl::Source::Wrapper source;
        utl::Source_position position;
        std::string_view     string;

        static auto make(utl::Source::Wrapper, Compile_info&) -> Lex_state;
    };

    auto lex(Lex_state&) -> Token;

} // namespace kieli
