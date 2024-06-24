#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

libparse::Context::Context(cst::Node_arena& arena, kieli::Lex_state const state)
    : m_lex_state { state }
    , m_node_arena { arena }
    , m_special_identifiers {
        .plus     = kieli::Identifier { state.compile_info.operator_pool.make("+") },
        .asterisk = kieli::Identifier { state.compile_info.operator_pool.make("*") },
    }
{}

auto libparse::Context::is_finished() -> bool
{
    return peek().type == Token::Type::end_of_input;
}

auto libparse::Context::peek() -> Token
{
    if (m_token_index == m_cached_tokens.size()) {
        return m_cached_tokens.emplace_back(kieli::lex(m_lex_state));
    }
    return m_cached_tokens[m_token_index];
}

auto libparse::Context::extract() -> Token
{
    Token token = peek();
    ++m_token_index;
    m_previous_token_range = token.range;
    return token;
}

auto libparse::Context::try_extract(Token::Type const type) -> std::optional<Token>
{
    return peek().type == type ? std::optional(extract()) : std::nullopt;
}

auto libparse::Context::require_extract(Token::Type const type) -> Token
{
    if (auto token = try_extract(type)) {
        return token.value();
    }
    error_expected(Token::description(type));
}

auto libparse::Context::stage() const -> Stage
{
    return { .old_token_index = m_token_index };
}

auto libparse::Context::unstage(Stage const stage) -> void
{
    cpputil::always_assert(stage.old_token_index < m_cached_tokens.size());
    m_token_index = stage.old_token_index;
}

auto libparse::Context::commit(Stage const stage) -> void
{
    if (stage.old_token_index == 0) {
        m_cached_tokens.erase(
            m_cached_tokens.begin(),
            m_cached_tokens.begin()
                + utl::safe_cast<decltype(m_cached_tokens)::difference_type>(m_token_index));
        m_token_index = 0;
    }
}

auto libparse::Context::up_to_current(kieli::Range const range) const -> kieli::Range
{
    cpputil::always_assert(m_previous_token_range.has_value());
    return { range.start, m_previous_token_range.value().stop };
}

auto libparse::Context::error_expected(
    kieli::Range const         error_range,
    std::string_view const     description,
    std::optional<std::string> help_note) -> void
{
    kieli::fatal_error(
        compile_info(),
        source(),
        error_range,
        std::format("Found {} where {} was expected", Token::description(peek().type), description),
        std::move(help_note));
}

auto libparse::Context::error_expected(std::string_view const description) -> void
{
    error_expected(peek().range, description);
}

auto libparse::Context::compile_info() -> kieli::Compile_info&
{
    return m_lex_state.compile_info;
}

auto libparse::Context::special_identifiers() const -> Special_identifiers
{
    return m_special_identifiers;
}

auto libparse::Context::source() const -> kieli::Source_id
{
    return m_lex_state.source;
}
