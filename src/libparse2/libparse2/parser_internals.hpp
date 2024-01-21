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

        // Consume the current token if it matches `type`.
        [[nodiscard]] auto try_extract(kieli::Token2::Type type) -> std::optional<kieli::Token2>;

        // Consume the current token if it matches `type`, otherwise an expectation failure error.
        [[nodiscard]] auto extract_required(kieli::Token2::Type type) -> kieli::Token2;

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
    private:
        auto consume(kieli::Token2) -> kieli::Token2;
    };

    auto parse_block_expression(Context&) -> std::optional<cst::Expression>;
    auto parse_expression(Context&) -> std::optional<cst::Expression>;
    auto parse_definition(Context&) -> std::optional<cst::Definition>;
    auto parse_pattern(Context&) -> std::optional<cst::Pattern>;
    auto parse_type(Context&) -> std::optional<cst::Type>;

    auto parse_template_parameters(Context&) -> std::optional<cst::Template_parameters>;
    auto parse_template_parameter(Context&) -> std::optional<cst::Template_parameter>;
    auto parse_template_arguments(Context&) -> std::optional<cst::Template_arguments>;
    auto parse_template_argument(Context&) -> std::optional<cst::Template_argument>;

    auto parse_function_parameters(Context&) -> std::optional<cst::Function_parameters>;
    auto parse_function_parameter(Context&) -> std::optional<cst::Function_parameter>;
    auto parse_function_arguments(Context&) -> std::optional<cst::Function_arguments>;
    auto parse_function_argument(Context&) -> std::optional<cst::Function_argument>;

    template <class Function>
    concept parser = requires(Function const function, Context context) {
        // clang-format off
        { function(context) } -> utl::specialization_of<std::optional>;
        // clang-format on
    };

    template <parser auto parser>
    using Parser_target = decltype(parser(std::declval<Context&>()))::value_type;

    template <parser auto parser, utl::Metastring description>
    auto extract_required(Context& context) -> Parser_target<parser>
    {
        if (auto result = parser(context)) {
            return std::move(result.value());
        }
        context.error_expected(description.view());
    }

    template <
        parser auto         parser,
        utl::Metastring     description,
        kieli::Token2::Type open_type,
        kieli::Token2::Type close_type>
    auto parse_surrounded(Context& context) -> std::optional<cst::Surrounded<Parser_target<parser>>>
    {
        return context.try_extract(open_type).transform([&](kieli::Token2 const& open) {
            return cst::Surrounded<Parser_target<parser>> {
                .value       = extract_required<parser, description>(context),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(context.extract_required(close_type)),
            };
        });
    }

    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_parenthesized = parse_surrounded<
        parser,
        description,
        kieli::Token2::Type::paren_open,
        kieli::Token2::Type::paren_close>;
    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_braced = parse_surrounded<
        parser,
        description,
        kieli::Token2::Type::brace_open,
        kieli::Token2::Type::brace_close>;
    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_bracketed = parse_surrounded<
        parser,
        description,
        kieli::Token2::Type::bracket_open,
        kieli::Token2::Type::bracket_close>;

    template <
        parser auto                 parser,
        utl::Metastring             description,
        kieli::Token2::Token2::Type separator_type>
    auto extract_separated_zero_or_more(Context& context)
        -> cst::Separated_sequence<Parser_target<parser>>
    {
        cst::Separated_sequence<Parser_target<parser>> sequence;
        if (auto first_element = parser(context)) {
            sequence.elements.push_back(std::move(first_element.value()));
            while (auto separator = context.try_extract(separator_type)) {
                sequence.separator_tokens.push_back(cst::Token::from_lexical(separator.value()));
                sequence.elements.push_back(extract_required<parser, description>(context));
            }
        }
        return sequence;
    }

    template <
        parser auto                 parser,
        utl::Metastring             description,
        kieli::Token2::Token2::Type separator>
    auto parse_separated_one_or_more(Context& context)
        -> std::optional<cst::Separated_sequence<Parser_target<parser>>>
    {
        auto sequence = extract_separated_zero_or_more<parser, description, separator>(context);
        if (sequence.elements.empty()) {
            return std::nullopt;
        }
        return sequence;
    }

    template <parser auto parser, utl::Metastring description>
    constexpr auto extract_comma_separated_zero_or_more
        = extract_separated_zero_or_more<parser, description, kieli::Token2::Type::comma>;
    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_comma_separated_one_or_more
        = parse_separated_one_or_more<parser, description, kieli::Token2::Type::comma>;

    template <kieli::Token2::Type type, bool is_upper = (type == kieli::Token2::Type::upper_name)>
    auto parse_name(Context& context) -> std::optional<kieli::Basic_name<is_upper>>
    {
        return context.try_extract(type).transform([&](kieli::Token2 const& token) {
            return kieli::Basic_name<is_upper> {
                .identifier  = token.value_as<kieli::Identifier>(),
                .source_view = token.source_view,
            };
        });
    }

    constexpr auto parse_lower_name = parse_name<kieli::Token2::Type::lower_name>;
    constexpr auto parse_upper_name = parse_name<kieli::Token2::Type::upper_name>;

} // namespace libparse2
