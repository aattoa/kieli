#include "cpputil/util.hpp"
#include <liblex2/state.hpp>

namespace {
    auto source_view_for(kieli::Lex2_state const& state, std::string_view const view) noexcept
        -> utl::Source_view
    {
        utl::Source_position start_position;
        for (char const* ptr = liblex2::source_begin(state); ptr != view.data(); ++ptr) {
            start_position.advance_with(*ptr);
        }
        utl::Source_position stop_position = start_position;
        for (char const c : view) {
            stop_position.advance_with(c);
        }
        return utl::Source_view { state.source, view, start_position, stop_position };
    }
} // namespace

auto liblex2::source_begin(const kieli::Lex2_state& state) noexcept -> char const*
{
    return state.source->string().data();
}

auto liblex2::source_end(const kieli::Lex2_state& state) noexcept -> char const*
{
    return source_begin(state) + state.source->string().size();
}

auto liblex2::current(const kieli::Lex2_state& state) noexcept -> char
{
    cpputil::always_assert(!state.string.empty());
    return state.string.front();
}

auto liblex2::extract_current(kieli::Lex2_state& state) noexcept -> char
{
    cpputil::always_assert(!state.string.empty());
    char const character = state.string.front();
    state.string.remove_prefix(1);
    return character;
}

auto liblex2::advance(kieli::Lex2_state& state, std::size_t const offset) noexcept -> void
{
    cpputil::always_assert(offset <= state.string.size());
    for (char const character : state.string.substr(0, offset)) {
        state.position.advance_with(character);
    }
    state.string.remove_prefix(offset);
}

auto liblex2::try_consume(kieli::Lex2_state& state, char const character) noexcept -> bool
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

auto liblex2::try_consume(kieli::Lex2_state& state, std::string_view const string) noexcept -> bool
{
    if (state.string.starts_with(string)) {
        advance(state, string.size());
        return true;
    }
    return false;
}

auto liblex2::make_string_literal(kieli::Lex2_state const& state, std::string_view const string)
    -> kieli::String
{
    return kieli::String { state.compile_info.string_literal_pool.make(string) };
}

auto liblex2::make_operator_identifier(
    kieli::Lex2_state const& state, std::string_view const string) -> kieli::Identifier
{
    assert(!string.empty());
    return kieli::Identifier { state.compile_info.operator_pool.make(string) };
}

auto liblex2::make_identifier(kieli::Lex2_state const& state, std::string_view const string)
    -> kieli::Identifier
{
    assert(!string.empty());
    return kieli::Identifier { state.compile_info.identifier_pool.make(string) };
}

auto liblex2::error(
    kieli::Lex2_state const& state,
    std::string_view const   position,
    std::string_view const   message) -> std::unexpected<Token_extraction_failure>
{
    state.compile_info.diagnostics.emit(
        cppdiag::Severity::error, source_view_for(state, position), "{}", message);
    return std::unexpected { Token_extraction_failure {} };
}

auto liblex2::error(
    kieli::Lex2_state const& state,
    char const* const        position,
    std::string_view const   message) -> std::unexpected<Token_extraction_failure>
{
    return error(state, { position, position }, message);
}

auto liblex2::error(kieli::Lex2_state const& state, std::string_view const message)
    -> std::unexpected<Token_extraction_failure>
{
    cpputil::always_assert(state.string.data() != nullptr);
    return error(state, state.string.data(), message);
}
