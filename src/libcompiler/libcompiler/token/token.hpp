#ifndef KIELI_LIBCOMPILER_TOKEN
#define KIELI_LIBCOMPILER_TOKEN

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

namespace ki {

    enum struct Token_type : std::uint8_t {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) identifier,
#include <libcompiler/token/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    };

    struct Token {
        Token_type type {};
        Range      range;
        utl::View  view;
    };

    [[nodiscard]] auto token_description(Token_type type) -> std::string_view;
    [[nodiscard]] auto token_type_string(Token_type type) -> std::string_view;

} // namespace ki

#endif // KIELI_LIBCOMPILER_TOKEN
