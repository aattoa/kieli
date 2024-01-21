#pragma once

#include <liblex2/lex.hpp>
#include <libparse2/cst.hpp>

namespace libparse2 {

    class Context {
        kieli::Lex2_state               m_lex_state;
        std::optional<utl::Source_view> m_previous_token_source_view;
        std::vector<kieli::Token2>      m_cached_tokens;
        cst::Node_arena&                m_node_arena;
    public:
        explicit Context(cst::Node_arena&, kieli::Lex2_state);

        // Check whether the current token is the end-of-input token.
        [[nodiscard]] auto is_finished() -> bool;

        // Inspect the current token without consuming it.
        [[nodiscard]] auto peek() -> kieli::Token2;

        // Consume the current token.
        [[nodiscard]] auto extract() -> kieli::Token2;

        // Cache `token` so that the next call to `extract` will return it.
        auto restore(kieli::Token2 token) -> void;

        // Source view from `start` up to (but not including) the current token.
        auto up_to_current(utl::Source_view start) const -> utl::Source_view;

        // Emit an error that describes an expectation failure:
        // Encountered `error_view` where `description` was expected.
        [[noreturn]] auto error_expected(utl::Source_view error_view, std::string_view description)
            -> void;

        // Emit an error that describes an expectation failure:
        // Encountered the current token where `description` was expected.
        [[noreturn]] auto error_expected(std::string_view description) -> void;

        template <cst::node Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node>
        {
            return m_node_arena.wrap<Node>(static_cast<Node&&>(node));
        }
    };

    auto parse_definition(Context&) -> std::optional<cst::Definition>;
    auto parse_expression(Context&) -> std::optional<cst::Expression>;
    auto parse_pattern(Context&) -> std::optional<cst::Pattern>;
    auto parse_type(Context&) -> std::optional<cst::Type>;

} // namespace libparse2
