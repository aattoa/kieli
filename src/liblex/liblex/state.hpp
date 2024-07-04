#pragma once

#include <liblex/lex.hpp>

namespace liblex {
    struct Error {};

    template <class T>
    using Expected = std::expected<T, Error>;

    [[nodiscard]] auto source_begin(kieli::Lex_state const&) noexcept -> char const*;
    [[nodiscard]] auto source_end(kieli::Lex_state const&) noexcept -> char const*;
    [[nodiscard]] auto position(kieli::Lex_state const&) noexcept -> kieli::Position;
    [[nodiscard]] auto current(kieli::Lex_state const&) noexcept -> char;
    [[nodiscard]] auto extract_current(kieli::Lex_state&) noexcept -> char;

    [[nodiscard]] auto try_consume(kieli::Lex_state&, char) noexcept -> bool;
    [[nodiscard]] auto try_consume(kieli::Lex_state&, std::string_view) noexcept -> bool;

    [[nodiscard]] auto error(
        kieli::Lex_state const&    state,
        std::string_view           position,
        std::string                message,
        std::optional<std::string> help_note = std::nullopt) -> std::unexpected<Error>;

    [[nodiscard]] auto error(kieli::Lex_state const& state, std::string message)
        -> std::unexpected<Error>;

    auto advance(kieli::Lex_state&, std::size_t offset = 1) noexcept -> void;

    auto consume(kieli::Lex_state& state, std::predicate<char> auto const& predicate) noexcept
        -> void
    {
        while (!state.string.empty() && predicate(state.string.front())) {
            state.position.advance_with(state.string.front());
            state.string.remove_prefix(1);
        }
    }

    [[nodiscard]] auto extract(kieli::Lex_state& state, std::predicate<char> auto const& predicate)
        noexcept -> std::string_view
    {
        std::string_view const string = state.string;
        consume(state, predicate);
        return string.substr(0, string.size() - state.string.size());
    }

    auto make_string_literal(kieli::Lex_state const&, std::string_view) -> kieli::String;
    auto make_operator_identifier(kieli::Lex_state const&, std::string_view) -> kieli::Identifier;
    auto make_identifier(kieli::Lex_state const&, std::string_view) -> kieli::Identifier;
} // namespace liblex
