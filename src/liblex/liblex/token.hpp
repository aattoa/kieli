#ifndef KIELI_LIBLEX_TOKEN
#define KIELI_LIBLEX_TOKEN

#include <libutl/utilities.hpp>
#include <libcompiler/lsp.hpp>

namespace ki::lex {

    // Lexical token type.
    enum struct Type : std::uint8_t {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) identifier,
#include <liblex/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    };

    // Lexical token.
    struct Token {
        Type       type {};
        lsp::Range range;
        utl::View  view;
    };

    // A human readable description of a token.
    [[nodiscard]] auto token_description(Type type) -> std::string_view;

    // A short description of a token, mostly corresponding to its spelling.
    [[nodiscard]] auto token_type_string(Type type) -> std::string_view;

    // Best effort syntax highlighting for tokens skipped during recovery.
    [[nodiscard]] auto recovery_semantic_token(Type type)
        -> std::optional<lsp::Semantic_token_type>;

} // namespace ki::lex

#endif // KIELI_LIBLEX_TOKEN
