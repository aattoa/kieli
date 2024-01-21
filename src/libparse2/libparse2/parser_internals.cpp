#include <libutl/common/utilities.hpp>
#include <libparse2/parser_internals.hpp>

namespace {
    auto do_extract(std::vector<kieli::Token2>& cache, kieli::Lex2_state& lex_state)
        -> kieli::Token2
    {
        if (!cache.empty()) {
            kieli::Token2 token = cache.back();
            cache.pop_back();
            return token;
        }
        return kieli::lex2(lex_state);
    }
} // namespace

libparse2::Context::Context(cst::Node_arena& arena, kieli::Lex2_state state)
    : m_lex_state { std::move(state) }
    , m_node_arena { arena }
{}

auto libparse2::Context::is_finished() -> bool
{
    return peek().type == kieli::Token2::Type::end_of_input;
}

auto libparse2::Context::peek() -> kieli::Token2
{
    if (m_cached_tokens.empty()) {
        m_cached_tokens.push_back(kieli::lex2(m_lex_state));
    }
    return m_cached_tokens.back();
}

auto libparse2::Context::extract() -> kieli::Token2
{
    kieli::Token2 token          = do_extract(m_cached_tokens, m_lex_state);
    m_previous_token_source_view = token.source_view;
    return token;
}

auto libparse2::Context::restore(kieli::Token2 const token) -> void
{
    m_cached_tokens.push_back(token);
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
        error_view,
        "Expected {}, but found {}",
        description,
        kieli::Token2::description(peek().type));
}

auto libparse2::Context::error_expected(std::string_view const description) -> void
{
    error_expected(peek().source_view, description);
}
