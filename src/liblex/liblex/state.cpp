#include "cpputil/util.hpp"
#include <liblex/state.hpp>

namespace {
    auto range_for(kieli::Lex_state const& state, std::string_view const view) noexcept
        -> kieli::Range
    {
        kieli::Position start;
        for (char const* ptr = liblex::source_begin(state); ptr != view.data(); ++ptr) {
            start.advance_with(*ptr);
        }
        kieli::Position stop = start;
        for (char const c : view) {
            stop.advance_with(c);
        }
        return kieli::Range { start, stop };
    }
} // namespace

auto liblex::source_begin(const kieli::Lex_state& state) noexcept -> char const*
{
    return state.db.sources[state.source].content.data();
}

auto liblex::source_end(const kieli::Lex_state& state) noexcept -> char const*
{
    return state.string.data() + state.string.size();
}

auto liblex::current(const kieli::Lex_state& state) noexcept -> char
{
    cpputil::always_assert(!state.string.empty());
    return state.string.front();
}

auto liblex::extract_current(kieli::Lex_state& state) noexcept -> char
{
    cpputil::always_assert(!state.string.empty());
    char const character = state.string.front();
    advance(state);
    return character;
}

auto liblex::advance(kieli::Lex_state& state, std::size_t const offset) noexcept -> void
{
    cpputil::always_assert(offset <= state.string.size());
    for (char const character : state.string.substr(0, offset)) {
        state.position.advance_with(character);
    }
    state.string.remove_prefix(offset);
}

auto liblex::try_consume(kieli::Lex_state& state, char const character) noexcept -> bool
{
    if (state.string.empty()) {
        return false;
    }
    if (current(state) == character) {
        state.position.advance_with(character);
        state.string.remove_prefix(1);
        return true;
    }
    return false;
}

auto liblex::try_consume(kieli::Lex_state& state, std::string_view const string) noexcept -> bool
{
    if (state.string.starts_with(string)) {
        advance(state, string.size());
        return true;
    }
    return false;
}

auto liblex::make_string_literal(kieli::Lex_state const& state, std::string_view const string)
    -> kieli::String
{
    return kieli::String { state.db.string_pool.add(string) };
}

auto liblex::make_operator_identifier(kieli::Lex_state const& state, std::string_view const string)
    -> kieli::Identifier
{
    assert(!string.empty());
    return kieli::Identifier { state.db.string_pool.add(string) };
}

auto liblex::make_identifier(kieli::Lex_state const& state, std::string_view const string)
    -> kieli::Identifier
{
    assert(!string.empty());
    return kieli::Identifier { state.db.string_pool.add(string) };
}

auto liblex::error(
    kieli::Lex_state const&    state,
    std::string_view const     position,
    std::string                message,
    std::optional<std::string> help_note) -> std::unexpected<Error>
{
    kieli::emit_diagnostic(
        cppdiag::Severity::error,
        state.db,
        state.source,
        range_for(state, position),
        std::move(message),
        std::move(help_note));
    return std::unexpected { Error {} };
}

auto liblex::error(kieli::Lex_state const& state, std::string message) -> std::unexpected<Error>
{
    cpputil::always_assert(state.string.data() != nullptr);
    return error(state, { state.string.data(), 1 }, std::move(message));
}
