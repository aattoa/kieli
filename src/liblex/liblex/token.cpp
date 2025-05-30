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
    }
    cpputil::unreachable();
}

auto ki::lex::token_type_string(Type const type) -> std::string_view
{
    switch (type) {
#define KIELI_X_TOKEN_DO(identifier, spelling, description) \
    case Type::identifier: return spelling;
#include <liblex/token-x-macro-table.hpp>
#undef KIELI_X_TOKEN_DO
    }
    cpputil::unreachable();
}

auto ki::lex::recovery_semantic_token(Type const type) -> std::optional<lsp::Semantic_token_type>
{
    using enum lsp::Semantic_token_type;
    switch (type) {
    case Type::Unterminated_comment: return Comment;
    case Type::Unterminated_string:
    case Type::String:               return String;
    case Type::Integer:
    case Type::Floating:
    case Type::Boolean:              return Number;
    case Type::Lower_name:
    case Type::Upper_name:           return Variable;
    case Type::Underscore:
    case Type::Dot:
    case Type::Comma:
    case Type::Colon:
    case Type::Semicolon:
    case Type::Double_colon:
    case Type::Ampersand:
    case Type::Asterisk:
    case Type::Plus:
    case Type::Equals:
    case Type::Question:
    case Type::Exclamation:
    case Type::Pipe:
    case Type::Left_arrow:
    case Type::Right_arrow:
    case Type::Paren_open:
    case Type::Paren_close:
    case Type::Brace_open:
    case Type::Brace_close:
    case Type::Bracket_open:
    case Type::Bracket_close:
    case Type::Operator:             return Operator_name;
    case Type::Let:
    case Type::Mut:
    case Type::Immut:
    case Type::If:
    case Type::Else:
    case Type::For:
    case Type::In:
    case Type::While:
    case Type::Loop:
    case Type::Continue:
    case Type::Break:
    case Type::Match:
    case Type::Ret:
    case Type::Fn:
    case Type::Enum:
    case Type::Struct:
    case Type::Concept:
    case Type::Impl:
    case Type::Alias:
    case Type::Import:
    case Type::Export:
    case Type::Module:
    case Type::Sizeof:
    case Type::Typeof:
    case Type::Where:
    case Type::Dyn:
    case Type::Macro:
    case Type::Global:
    case Type::Defer:                return Keyword;
    case Type::Invalid_character:
    case Type::Lambda:
    case Type::End_of_input:         return std::nullopt;
    }
    cpputil::unreachable();
}
