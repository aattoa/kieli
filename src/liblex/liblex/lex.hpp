#pragma once

#include <libutl/utilities.hpp>
#include <libphase/phase.hpp>
#include <liblex/token.hpp>

namespace kieli {

    struct Lex_state {
        Compile_info&        compile_info;
        utl::Source_id       source;
        utl::Source_position position;
        std::string_view     string;

        static auto make(utl::Source_id, Compile_info&) -> Lex_state;
    };

    auto lex(Lex_state&) -> Token;

} // namespace kieli
