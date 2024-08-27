#include <libutl/utilities.hpp>
#include <libcompiler/token/token.hpp>

// Evil x-macro preprocessor hack ensures the switches are always correct.

auto kieli::token_description(Token_type const type) -> std::string_view
{
    switch (type) {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) \
    case Token_type::identifier: return description;
#include <libcompiler/token/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    default: cpputil::unreachable();
    }
}

auto kieli::token_type_string(Token_type const type) -> std::string_view
{
    switch (type) {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) \
    case Token_type::identifier: return spelling;
#include <libcompiler/token/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    default: cpputil::unreachable();
    }
}

static_assert(std::is_trivially_copyable_v<kieli::Token>);
