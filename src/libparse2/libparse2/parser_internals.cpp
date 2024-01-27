#include <libutl/common/utilities.hpp>
#include <libparse2/parser_internals.hpp>

libparse2::Context::Context(cst::Node_arena& arena, kieli::Lex2_state const state)
    : m_lex_state { state }
    , m_node_arena { arena }
    , m_special_identifiers {
        .plus     = kieli::Identifier { state.compile_info.operator_pool.make("+") },
        .asterisk = kieli::Identifier { state.compile_info.operator_pool.make("*") },
    }
{}

auto libparse2::Context::is_finished() -> bool
{
    return peek().type == Token::Type::end_of_input;
}

auto libparse2::Context::peek() -> Token
{
    if (m_token_index == m_cached_tokens.size()) {
        return m_cached_tokens.emplace_back(kieli::lex2(m_lex_state));
    }
    return m_cached_tokens[m_token_index];
}

auto libparse2::Context::extract() -> Token
{
    Token token = peek();
    ++m_token_index;
    m_previous_token_source_view = token.source_view;
    return token;
}

auto libparse2::Context::try_extract(Token::Type const type) -> std::optional<Token>
{
    return peek().type == type ? std::optional(extract()) : std::nullopt;
}

auto libparse2::Context::require_extract(Token::Type const type) -> Token
{
    if (auto token = try_extract(type)) {
        return token.value();
    }
    error_expected(Token::description(type));
}

auto libparse2::Context::stage() const -> Stage
{
    return { .old_token_index = m_token_index };
}

auto libparse2::Context::unstage(Stage const stage) -> void
{
    cpputil::always_assert(stage.old_token_index < m_cached_tokens.size());
    m_token_index = stage.old_token_index;
}

auto libparse2::Context::commit(Stage const stage) -> void
{
    if (stage.old_token_index == 0) {
        m_cached_tokens.erase(
            m_cached_tokens.begin(),
            m_cached_tokens.begin()
                + utl::safe_cast<decltype(m_cached_tokens)::difference_type>(m_token_index));
        m_token_index = 0;
    }
}

auto libparse2::Context::up_to_current(utl::Source_view const start) const -> utl::Source_view
{
    cpputil::always_assert(m_previous_token_source_view.has_value());
    return start.combine_with(m_previous_token_source_view.value());
}

auto libparse2::Context::error_expected(
    utl::Source_view const error_view, std::string_view const description) -> void
{
    m_lex_state.compile_info.diagnostics.error(
        error_view, "Expected {}, but found {}", description, Token::description(peek().type));
}

auto libparse2::Context::error_expected(std::string_view const description) -> void
{
    error_expected(peek().source_view, description);
}

auto libparse2::Context::compile_info() -> kieli::Compile_info&
{
    return m_lex_state.compile_info;
}

auto libparse2::Context::special_identifiers() const -> Special_identifiers
{
    return m_special_identifiers;
}
