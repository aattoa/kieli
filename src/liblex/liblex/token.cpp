#include <libutl/utilities.hpp>
#include <liblex/token.hpp>

// Evil x-macro preprocessor hack ensures the switches are always correct.

auto ki::lex::token_description(Type const type) -> std::string_view
{
    switch (type) {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) \
    case Type::identifier: return description;
#include <liblex/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    default: cpputil::unreachable();
    }
}

auto ki::lex::token_type_string(Type const type) -> std::string_view
{
    switch (type) {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) \
    case Type::identifier: return spelling;
#include <liblex/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    default: cpputil::unreachable();
    }
}
