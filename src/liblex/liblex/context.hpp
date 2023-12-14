#pragma once

#include <liblex/lex.hpp>

namespace liblex {
    struct Token_extraction_failure {};

    template <class T>
    using Expected = std::expected<T, Token_extraction_failure>;

    class [[nodiscard]] Context {
        kieli::Compile_info& m_compile_info;
        utl::Source::Wrapper m_source;
        char const*          m_source_begin {};
        char const*          m_source_end {};
        char const*          m_pointer {};
        utl::Source_position m_position {};

        [[nodiscard]] auto source_view_for(std::string_view) const noexcept -> utl::Source_view;
        [[nodiscard]] auto remaining_input_size() const noexcept -> utl::Usize;
    public:
        explicit Context(utl::Source::Wrapper, kieli::Compile_info&) noexcept;

        [[nodiscard]] auto source() const noexcept -> utl::Source::Wrapper;
        [[nodiscard]] auto source_begin() const noexcept -> char const*;
        [[nodiscard]] auto source_end() const noexcept -> char const*;
        [[nodiscard]] auto pointer() const noexcept -> char const*;
        [[nodiscard]] auto position() const noexcept -> utl::Source_position;
        [[nodiscard]] auto is_finished() const noexcept -> bool;
        [[nodiscard]] auto current() const noexcept -> char;
        [[nodiscard]] auto extract_current() noexcept -> char;

        [[nodiscard]] auto try_consume(char) noexcept -> bool;
        [[nodiscard]] auto try_consume(std::string_view) noexcept -> bool;

        [[nodiscard]] auto error(std::string_view position, std::string_view message)
            -> std::unexpected<Token_extraction_failure>;
        [[nodiscard]] auto error(char const* position, std::string_view message)
            -> std::unexpected<Token_extraction_failure>;
        [[nodiscard]] auto error(std::string_view message)
            -> std::unexpected<Token_extraction_failure>;

        auto advance(utl::Usize offset = 1) noexcept -> void;

        auto consume(std::predicate<char> auto const& predicate) noexcept -> void
        {
            while (m_pointer != m_source_end && predicate(*m_pointer)) {
                m_position.advance_with(*m_pointer++);
            }
        }

        [[nodiscard]] auto extract(std::predicate<char> auto const& predicate) noexcept
            -> std::string_view
        {
            char const* const pointer = m_pointer;
            consume(predicate);
            return { pointer, m_pointer };
        }

        auto make_string_literal(std::string_view) -> kieli::String;
        auto make_operator_identifier(std::string_view) -> kieli::Identifier;
        auto make_identifier(std::string_view) -> kieli::Identifier;
    };
} // namespace liblex
