#include "cpputil/util.hpp"
#include <liblex/state.hpp>

namespace {
    auto source_range_for(kieli::Lex_state const& state, std::string_view const view) noexcept
        -> utl::Source_range
    {
        utl::Source_position start;
        for (char const* ptr = liblex::source_begin(state); ptr != view.data(); ++ptr) {
            start.advance_with(*ptr);
        }
        utl::Source_position stop = start;
        for (char const c : view) {
            stop.advance_with(c);
        }
        return utl::Source_range { start, stop };
    }
} // namespace

auto liblex::source_begin(const kieli::Lex_state& state) noexcept -> char const*
{
    return state.source->string().data();
}

auto liblex::source_end(const kieli::Lex_state& state) noexcept -> char const*
{
    return source_begin(state) + state.source->string().size();
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
    state.string.remove_prefix(1);
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
    return kieli::String { state.compile_info.string_literal_pool.make(string) };
}

auto liblex::make_operator_identifier(kieli::Lex_state const& state, std::string_view const string)
    -> kieli::Identifier
{
    assert(!string.empty());
    return kieli::Identifier { state.compile_info.operator_pool.make(string) };
}

auto liblex::make_identifier(kieli::Lex_state const& state, std::string_view const string)
    -> kieli::Identifier
{
    assert(!string.empty());
    return kieli::Identifier { state.compile_info.identifier_pool.make(string) };
}

auto liblex::error(
    kieli::Lex_state const& state,
    std::string_view const  position,
    std::string_view const  message) -> std::unexpected<Token_extraction_failure>
{
    state.compile_info.diagnostics.emit(
        cppdiag::Severity::error, state.source, source_range_for(state, position), "{}", message);
    return std::unexpected { Token_extraction_failure {} };
}

auto liblex::error(
    kieli::Lex_state const& state,
    char const* const       position,
    std::string_view const  message) -> std::unexpected<Token_extraction_failure>
{
    return error(state, { position, position }, message);
}

auto liblex::error(kieli::Lex_state const& state, std::string_view const message)
    -> std::unexpected<Token_extraction_failure>
{
    cpputil::always_assert(state.string.data() != nullptr);
    return error(state, state.string.data(), message);
}
