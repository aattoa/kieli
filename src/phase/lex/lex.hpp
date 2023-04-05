#pragma once

#include "utl/utilities.hpp"
#include "utl/source.hpp"
#include "compiler/compiler.hpp"
#include "representation/token/token.hpp"


namespace compiler {

    struct [[nodiscard]] Lex_arguments {
        Compilation_info compilation_info;
        utl::Source      source;
    };

    struct [[nodiscard]] Lex_result {
        Compilation_info           compilation_info;
        std::vector<Lexical_token> tokens;
        utl::Source                source;
    };

    auto lex(Lex_arguments&&) -> Lex_result;

}
