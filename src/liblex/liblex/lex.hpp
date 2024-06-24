#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/token/token.hpp>

namespace kieli {

    struct Lex_state {
        Compile_info&    compile_info;
        Source_id        source;
        Position         position;
        std::string_view string;

        static auto make(Source_id, Compile_info&) -> Lex_state;
    };

    auto lex(Lex_state&) -> Token;

} // namespace kieli
