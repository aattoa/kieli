#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

namespace kieli {
    using Token_variant = std::variant<
        std::monostate, //
        Integer,
        Floating,
        Character,
        Boolean,
        String,
        Identifier>;

    enum struct Token_type {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) identifier,
#include <libcompiler/token/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    };

    struct [[nodiscard]] Token {
        Token_variant    variant;
        Token_type       type;
        std::string_view preceding_trivia;
        Range            range;

        template <typename T>
        [[nodiscard]] auto value_as() const -> T
        {
            T const* const pointer = std::get_if<T>(&variant);
            cpputil::always_assert(pointer != nullptr);
            return *pointer;
        }
    };

    [[nodiscard]] auto token_description(Token_type type) -> std::string_view;
    [[nodiscard]] auto token_type_string(Token_type type) -> std::string_view;
} // namespace kieli

template <>
struct std::formatter<kieli::Token_type> : std::formatter<std::string_view> {
    auto format(kieli::Token_type const type, auto& context) const
    {
        return std::formatter<std::string_view>::format(kieli::token_type_string(type), context);
    }
};

template <>
struct std::formatter<kieli::Token> : std::formatter<std::string_view> {
    auto format(kieli::Token const& token, auto& context) const
    {
        if (std::holds_alternative<std::monostate>(token.variant)) {
            return std::formatter<std::string_view>::format(
                kieli::token_type_string(token.type), context);
        }
        return std::format_to(context.out(), "({}: {})", token.type, token.variant);
    }
};
