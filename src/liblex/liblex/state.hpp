#pragma once

#include <liblex/lex.hpp>

namespace liblex {
    struct Error {};

    template <typename T>
    using Expected = std::expected<T, Error>;

    [[nodiscard]] auto source_begin(kieli::Lex_state const&) noexcept -> char const*;
    [[nodiscard]] auto source_end(kieli::Lex_state const&) noexcept -> char const*;
    [[nodiscard]] auto position(kieli::Lex_state const&) noexcept -> kieli::Position;
    [[nodiscard]] auto current(kieli::Lex_state const&) noexcept -> char;
    [[nodiscard]] auto extract_current(kieli::Lex_state&) noexcept -> char;

    [[nodiscard]] auto try_consume(kieli::Lex_state&, char) noexcept -> bool;
    [[nodiscard]] auto try_consume(kieli::Lex_state&, std::string_view) noexcept -> bool;

    [[nodiscard]] auto error(
        kieli::Lex_state const& state,
        std::string_view        position,
        std::string             message) -> std::unexpected<Error>;

    [[nodiscard]] auto error(kieli::Lex_state const& state, std::string message)
        -> std::unexpected<Error>;

    void advance(kieli::Lex_state&, std::size_t offset = 1) noexcept;

    void consume(kieli::Lex_state& state, std::predicate<char> auto const& predicate) noexcept
    {
        while (not state.text.empty() and predicate(state.text.front())) {
            state.position.advance_with(state.text.front());
            state.text.remove_prefix(1);
        }
    }

    [[nodiscard]] auto extract(kieli::Lex_state& state, std::predicate<char> auto const& predicate)
        noexcept -> std::string_view
    {
        std::string_view const string = state.text;
        consume(state, predicate);
        return string.substr(0, string.size() - state.text.size());
    }

    auto make_string_literal(kieli::Lex_state const&, std::string_view) -> kieli::String;
    auto make_operator_identifier(kieli::Lex_state const&, std::string_view) -> kieli::Identifier;
    auto make_identifier(kieli::Lex_state const&, std::string_view) -> kieli::Identifier;
} // namespace liblex
