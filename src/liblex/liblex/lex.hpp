#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/token/token.hpp>

namespace kieli {

    struct Lex_state {
        Database&        db;
        Document_id      document_id;
        Position         position;
        std::string_view text;
    };

    [[nodiscard]] auto lex(Lex_state&) -> Token;

    [[nodiscard]] auto lex_state(Database& db, Document_id document_id) -> Lex_state;

} // namespace kieli
