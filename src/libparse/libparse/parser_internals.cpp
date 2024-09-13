#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

libparse::Context::Context(cst::Arena& arena, kieli::Lex_state const state)
    : m_lex_state { state }
    , m_arena { arena }
    , m_special_identifiers {
        .plus     = kieli::Identifier { state.db.string_pool.add("+") },
        .asterisk = kieli::Identifier { state.db.string_pool.add("*") },
    }
{}

auto libparse::Context::is_finished() -> bool
{
    return peek().type == Token_type::end_of_input;
}

auto libparse::Context::peek() -> Token
{
    if (!m_next_token.has_value()) {
        m_next_token = kieli::lex(m_lex_state);
    }
    return m_next_token.value();
}

auto libparse::Context::extract() -> Token
{
    if (m_next_token.has_value()) {
        Token token = m_next_token.value();
        m_next_token.reset();
        m_previous_token_range = token.range;
        return token;
    }
    return kieli::lex(m_lex_state);
}

auto libparse::Context::try_extract(Token_type const type) -> std::optional<Token>
{
    return peek().type == type ? std::optional(extract()) : std::nullopt;
}

auto libparse::Context::require_extract(Token_type const type) -> Token
{
    if (auto token = try_extract(type)) {
        return token.value();
    }
    error_expected(kieli::token_description(type));
}

auto libparse::Context::error_expected(
    kieli::Range const error_range, std::string_view const description) -> void
{
    auto message = std::format(
        "Expected {}, but found {}", description, kieli::token_description(peek().type));
    kieli::add_error(db(), document_id(), error_range, std::move(message));
    throw std::runtime_error("fatal parse error"); // todo
}

auto libparse::Context::error_expected(std::string_view const description) -> void
{
    error_expected(peek().range, description);
}

auto libparse::Context::up_to_current(kieli::Range const range) const -> kieli::Range
{
    cpputil::always_assert(m_previous_token_range.has_value());
    return kieli::Range(range.start, m_previous_token_range.value().stop);
}

auto libparse::Context::token(Token const& token) -> cst::Token_id
{
    return m_arena.tokens.push(token.range, token.preceding_trivia);
}

auto libparse::Context::db() -> kieli::Database&
{
    return m_lex_state.db;
}

auto libparse::Context::cst() const -> cst::Arena&
{
    return m_arena;
}

auto libparse::Context::special_identifiers() const -> Special_identifiers
{
    return m_special_identifiers;
}

auto libparse::Context::document_id() const -> kieli::Document_id
{
    return m_lex_state.document_id;
}

auto libparse::name_from_token(Token const& token) -> kieli::Name
{
    return kieli::Name { token.value_as<kieli::Identifier>(), token.range };
}
