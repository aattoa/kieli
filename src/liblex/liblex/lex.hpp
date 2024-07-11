#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/token/token.hpp>

namespace kieli {

    struct Lex_state {
        Database&        db;
        Source_id        source;
        Position         position;
        std::string_view string;

        static auto make(Source_id source, Database& db) -> Lex_state;
    };

    auto lex(Lex_state&) -> Token;

} // namespace kieli
