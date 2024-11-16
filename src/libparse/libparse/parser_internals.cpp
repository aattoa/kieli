#include <libutl/utilities.hpp>
#include <libparse/parser_internals.hpp>

libparse::Context::Context(cst::Arena& arena, kieli::Lex_state const state)
    : m_lex_state { state }
    , m_special_identifiers {
        .plus     = kieli::Identifier { state.db.string_pool.add("+") },
        .asterisk = kieli::Identifier { state.db.string_pool.add("*") },
    }
    , m_arena { arena }
{}

auto libparse::Context::is_finished() -> bool
{
    return peek().type == Token_type::end_of_input;
}

auto libparse::Context::peek() -> Token
{
    if (not m_next_token.has_value()) {
        m_next_token = kieli::lex(m_lex_state);
    }
    return m_next_token.value();
}

auto libparse::Context::extract() -> Token
{
    Token token = peek();
    m_next_token.reset();
    m_previous_token_range = token.range;
    return token;
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

void libparse::Context::error_expected(
    kieli::Range const error_range, std::string_view const description)
{
    auto message = std::format(
        "Expected {}, but found {}", description, kieli::token_description(peek().type));
    kieli::add_error(db(), document_id(), error_range, std::move(message));
    throw std::runtime_error("fatal parse error"); // todo
}

void libparse::Context::error_expected(std::string_view const description)
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

auto libparse::Context::document_id() const -> kieli::Document_id
{
    return m_lex_state.document_id;
}

auto libparse::Context::special_identifiers() const -> Special_identifiers
{
    return m_special_identifiers;
}

auto libparse::Context::semantic_tokens() -> std::vector<kieli::Semantic_token>&
{
    return m_semantic_tokens;
}

void libparse::Context::add_semantic_token(kieli::Range range, kieli::Semantic_token_type type)
{
    auto& prev = m_previous_semantic_token_position;
    if (range.start.line != range.stop.line) {
        return; // TODO
    }
    cpputil::always_assert(range.start.column <= range.stop.column);
    cpputil::always_assert(prev.line <= range.start.line);
    cpputil::always_assert(prev.line != range.start.line || prev.column <= range.start.column);
    if (range.start.column != range.stop.column) {
        if (range.start.line != prev.line) {
            prev.column = 0;
        }
        m_semantic_tokens.push_back({
            .delta_line   = range.start.line - prev.line,
            .delta_column = range.start.column - prev.column,
            .length       = range.stop.column - range.start.column,
            .type         = type,
        });
        prev = range.start;
    }
}

void libparse::Context::add_keyword(Token const& token)
{
    // TODO: register token type for hover info
    add_semantic_token(token.range, Semantic::keyword);
}

void libparse::Context::add_punctuation(Token const& token)
{
    // TODO: maybe introduce Semantic::punctuation
    add_semantic_token(token.range, Semantic::operator_name);
}

void libparse::Context::set_previous_path_head_semantic_token_offset(std::size_t offset)
{
    m_previous_path_head_semantic_token_offset = offset;
}

void libparse::Context::set_previous_path_head_semantic_type(Semantic const type)
{
    m_semantic_tokens.at(m_previous_path_head_semantic_token_offset).type = type;
}

auto libparse::name_from_token(Token const& token) -> kieli::Name
{
    return kieli::Name { token.value_as<kieli::Identifier>(), token.range };
}

auto libparse::extract_string(Context& context, Token const& literal) -> kieli::String
{
    context.add_semantic_token(literal.range, Semantic::string);
    auto const first_string = literal.value_as<kieli::String>();
    if (context.peek().type != Token_type::string_literal) {
        return first_string;
    }
    std::string combined_string { first_string.value.view() };
    while (auto const token = context.try_extract(Token_type::string_literal)) {
        context.add_semantic_token(token.value().range, Semantic::string);
        combined_string.append(token.value().value_as<kieli::String>().value.view());
    }
    return kieli::String { context.db().string_pool.add(combined_string) };
}

auto libparse::extract_integer(Context& context, Token const& literal) -> kieli::Integer
{
    context.add_semantic_token(literal.range, Semantic::number);
    return literal.value_as<kieli::Integer>();
}

auto libparse::extract_floating(Context& context, Token const& literal) -> kieli::Floating
{
    context.add_semantic_token(literal.range, Semantic::number);
    return literal.value_as<kieli::Floating>();
}

auto libparse::extract_character(Context& context, Token const& literal) -> kieli::Character
{
    context.add_semantic_token(literal.range, Semantic::string);
    return literal.value_as<kieli::Character>();
}

auto libparse::extract_boolean(Context& context, Token const& literal) -> kieli::Boolean
{
    context.add_keyword(literal);
    return literal.value_as<kieli::Boolean>();
}
