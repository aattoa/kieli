#pragma once

#include <liblex/lex.hpp>


namespace liblex {
    // Thrown on a lexical error, caught by the token extraction loop
    struct [[nodiscard]] Token_extraction_failure {};

    class [[nodiscard]] Context {
        compiler::Compilation_info& m_compilation_info;
        utl::Source::Wrapper        m_source;
        char const*                 m_source_begin {};
        char const*                 m_source_end {};
        char const*                 m_pointer {};
        utl::Source_position        m_position {};

        [[nodiscard]] auto source_view_for(std::string_view) const noexcept -> utl::Source_view;
        [[nodiscard]] auto remaining_input_size()            const noexcept -> utl::Usize;
    public:
        explicit Context(utl::Wrapper<utl::Source>, compiler::Compilation_info&) noexcept;

        [[nodiscard]] auto source()       const noexcept -> utl::Source::Wrapper;
        [[nodiscard]] auto source_begin() const noexcept -> char const*;
        [[nodiscard]] auto source_end()   const noexcept -> char const*;
        [[nodiscard]] auto pointer()      const noexcept -> char const*;
        [[nodiscard]] auto position()     const noexcept -> utl::Source_position;
        [[nodiscard]] auto is_finished()  const noexcept -> bool;
        [[nodiscard]] auto current()      const noexcept -> char;
        [[nodiscard]] auto extract_current()    noexcept -> char;

        [[nodiscard]] auto try_consume(char)             noexcept -> bool;
        [[nodiscard]] auto try_consume(std::string_view) noexcept -> bool;

        [[noreturn]] auto error(std::string_view, utl::diagnostics::Message_arguments) -> void;
        [[noreturn]] auto error(char const*, utl::diagnostics::Message_arguments) -> void;
        [[noreturn]] auto error(utl::diagnostics::Message_arguments) -> void;

        auto advance(utl::Usize offset = 1) noexcept -> void;

        auto consume(std::predicate<char> auto const& predicate) noexcept -> void {
            while (m_pointer != m_source_end && predicate(*m_pointer))
                m_position.advance_with(*m_pointer++);
        }
        [[nodiscard]]
        auto extract(std::predicate<char> auto const& predicate) noexcept -> std::string_view {
            char const* const pointer = m_pointer;
            consume(predicate);
            return { pointer, m_pointer };
        }

        auto make_string_literal(std::string_view) -> utl::Pooled_string;
        auto make_operator      (std::string_view) -> utl::Pooled_string;
        auto make_identifier    (std::string_view) -> utl::Pooled_string;
    };
}
