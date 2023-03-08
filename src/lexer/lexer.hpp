#pragma once

#include "utl/utilities.hpp"
#include "utl/source.hpp"
#include "utl/diagnostics.hpp"
#include "token.hpp"


namespace compiler {

    struct [[nodiscard]] Program_string_pool {
        String::Pool     literals;
        Identifier::Pool identifiers;
    };

    struct [[nodiscard]] Lex_result {
        std::vector<Lexical_token> tokens;
        utl::Source                 source;
        utl::diagnostics::Builder   diagnostics;
        Program_string_pool&       string_pool;
    };

    auto lex(
        utl::Source&&,
        Program_string_pool&,
        utl::diagnostics::Builder::Configuration = {}
    ) -> Lex_result;

}