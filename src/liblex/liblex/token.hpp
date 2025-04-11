#ifndef KIELI_LIBLEX_TOKEN
#define KIELI_LIBLEX_TOKEN

#include <libutl/utilities.hpp>
#include <libcompiler/lsp.hpp>

namespace ki::lex {

    enum struct Type : std::uint8_t {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) identifier,
#include <liblex/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    };

    struct Token {
        Type       type {};
        lsp::Range range;
        utl::View  view;
    };

    [[nodiscard]] auto token_description(Type type) -> std::string_view;
    [[nodiscard]] auto token_type_string(Type type) -> std::string_view;

} // namespace ki::lex

#endif // KIELI_LIBLEX_TOKEN
