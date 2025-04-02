#ifndef KIELI_LIBLEX_LEX
#define KIELI_LIBLEX_LEX

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/token/token.hpp>

namespace ki::lex {

    using Type = Token_type;

    struct State {
        Position         position;
        std::uint32_t    offset {};
        std::string_view text;
    };

    // Construct a lexical analysis state.
    [[nodiscard]] auto state(std::string_view text) -> State;

    // Compute the next token based on `state`.
    [[nodiscard]] auto next(State& state) -> Token;

} // namespace ki::lex

#endif // KIELI_LIBLEX_LEX
