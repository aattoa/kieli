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
    return kieli::document(state.db, state.document_id).text.data();
}

auto liblex::source_end(const kieli::Lex_state& state) noexcept -> char const*
{
    return state.text.data() + state.text.size();
}

auto liblex::current(const kieli::Lex_state& state) noexcept -> char
{
    cpputil::always_assert(!state.text.empty());
    return state.text.front();
}

auto liblex::extract_current(kieli::Lex_state& state) noexcept -> char
{
    cpputil::always_assert(!state.text.empty());
    char const character = state.text.front();
    advance(state);
    return character;
}

auto liblex::advance(kieli::Lex_state& state, std::size_t const offset) noexcept -> void
{
    cpputil::always_assert(offset <= state.text.size());
    for (char const character : state.text.substr(0, offset)) {
        state.position.advance_with(character);
    }
    state.text.remove_prefix(offset);
}

auto liblex::try_consume(kieli::Lex_state& state, char const character) noexcept -> bool
{
    if (state.text.empty()) {
        return false;
    }
    if (current(state) == character) {
        state.position.advance_with(character);
        state.text.remove_prefix(1);
        return true;
    }
    return false;
}

auto liblex::try_consume(kieli::Lex_state& state, std::string_view const string) noexcept -> bool
{
    if (state.text.starts_with(string)) {
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
    kieli::Diagnostic diagnostic {
        .message   = std::move(message),
        .help_note = std::move(help_note),
        .range     = range_for(state, position),
        .severity  = kieli::Severity::error,
    };
    kieli::document(state.db, state.document_id).diagnostics.push_back(std::move(diagnostic));
    return std::unexpected(Error {});
}

auto liblex::error(kieli::Lex_state const& state, std::string message) -> std::unexpected<Error>
{
    cpputil::always_assert(state.text.data() != nullptr);
    return error(state, { state.text.data(), 1 }, std::move(message));
}
