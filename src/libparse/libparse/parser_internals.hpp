#pragma once

#include <liblex/lex.hpp>
#include <libcompiler/cst/cst.hpp>

using kieli::Token;
using kieli::Token_type;

namespace libparse {

    namespace cst = kieli::cst;

    struct Special_identifiers {
        kieli::Identifier plus;
        kieli::Identifier asterisk;
    };

    class Context {
        kieli::Lex_state            m_lex_state;
        std::optional<Token>        m_next_token;
        std::optional<kieli::Range> m_previous_token_range;
        cst::Arena&                 m_arena;
        Special_identifiers         m_special_identifiers;
    public:
        explicit Context(cst::Arena&, kieli::Lex_state);

        // Check whether the current token is the end-of-input token.
        [[nodiscard]] auto is_finished() -> bool;

        // Inspect the current token without consuming it.
        [[nodiscard]] auto peek() -> Token;

        // Consume the current token.
        [[nodiscard]] auto extract() -> Token;

        // Consume the current token if it matches `type`.
        [[nodiscard]] auto try_extract(Token_type type) -> std::optional<Token>;

        // Consume the current token if it matches `type`, otherwise emit an error.
        [[nodiscard]] auto require_extract(Token_type type) -> Token;

        // Source view from `range` up to (but not including) the current token.
        [[nodiscard]] auto up_to_current(kieli::Range range) const -> kieli::Range;

        // Add a token to the CST arena, and return its id.
        [[nodiscard]] auto token(Token const& token) -> cst::Token_id;

        [[nodiscard]] auto db() -> kieli::Database&;
        [[nodiscard]] auto cst() const -> cst::Arena&;
        [[nodiscard]] auto special_identifiers() const -> Special_identifiers;
        [[nodiscard]] auto document_id() const -> kieli::Document_id;

        // Emit an error that describes an expectation failure:
        // Encountered `error_range` where `description` was expected.
        [[noreturn]] void error_expected(kieli::Range range, std::string_view description);

        // Emit an error that describes an expectation failure:
        // Encountered the current token where `description` was expected.
        [[noreturn]] void error_expected(std::string_view description);
    };

    auto parse_simple_path_root(Context&) -> std::optional<cst::Path_root>;
    auto parse_simple_path(Context&) -> std::optional<cst::Path>;
    auto parse_complex_path(Context&) -> std::optional<cst::Path>;
    auto extract_path(Context&, cst::Path_root) -> cst::Path;
    auto extract_concept_references(Context&) -> cst::Separated_sequence<cst::Path>;

    auto parse_block_expression(Context&) -> std::optional<cst::Expression_id>;
    auto parse_expression(Context&) -> std::optional<cst::Expression_id>;

    auto parse_top_level_pattern(Context&) -> std::optional<cst::Pattern_id>;
    auto parse_pattern(Context&) -> std::optional<cst::Pattern_id>;

    auto parse_template_parameters(Context&) -> std::optional<cst::Template_parameters>;
    auto parse_template_parameter(Context&) -> std::optional<cst::Template_parameter>;
    auto parse_template_arguments(Context&) -> std::optional<cst::Template_arguments>;
    auto parse_template_argument(Context&) -> std::optional<cst::Template_argument>;

    auto parse_function_parameters(Context&) -> std::optional<cst::Function_parameters>;
    auto parse_function_parameter(Context&) -> std::optional<cst::Function_parameter>;
    auto parse_function_arguments(Context&) -> std::optional<cst::Function_arguments>;

    auto parse_definition(Context&) -> std::optional<cst::Definition>;
    auto parse_mutability(Context&) -> std::optional<cst::Mutability>;
    auto parse_type_annotation(Context&) -> std::optional<cst::Type_annotation>;
    auto parse_type_root(Context&) -> std::optional<cst::Type_id>;
    auto parse_type(Context&) -> std::optional<cst::Type_id>;

    template <typename Function>
    concept parser = requires(Function const function, Context context) {
        { function(context) } -> utl::specialization_of<std::optional>;
    };

    template <parser auto parser>
    using Parser_target = decltype(parser(std::declval<Context&>()))::value_type;

    template <std::invocable<Context&> auto extract>
    auto pretend_parse(Context& context) -> std::optional<decltype(extract(context))>
    {
        return extract(context);
    }

    template <parser auto parser>
    auto require(Context& context, std::string_view const description) -> Parser_target<parser>
    {
        if (auto result = parser(context)) {
            return std::move(result.value());
        }
        context.error_expected(description);
    }

    template <parser auto parser, utl::Metastring description>
    auto require(Context& context) -> Parser_target<parser>
    {
        return require<parser>(context, description.view());
    }

    template <
        parser auto     parser,
        utl::Metastring description,
        Token_type      open_type,
        Token_type      close_type>
    auto parse_surrounded(Context& context) -> std::optional<cst::Surrounded<Parser_target<parser>>>
    {
        return context.try_extract(open_type).transform([&](Token const& open) {
            return cst::Surrounded<Parser_target<parser>> {
                .value       = require<parser>(context, description.view()),
                .open_token  = context.token(open),
                .close_token = context.token(context.require_extract(close_type)),
            };
        });
    }

    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_parenthesized
        = parse_surrounded<parser, description, Token_type::paren_open, Token_type::paren_close>;
    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_braced
        = parse_surrounded<parser, description, Token_type::brace_open, Token_type::brace_close>;
    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_bracketed = parse_surrounded<
        parser,
        description,
        Token_type::bracket_open,
        Token_type::bracket_close>;

    template <parser auto parser, utl::Metastring description, Token_type separator_type>
    auto extract_separated_zero_or_more(Context& context)
        -> cst::Separated_sequence<Parser_target<parser>>
    {
        cst::Separated_sequence<Parser_target<parser>> sequence;
        if (auto first_element = parser(context)) {
            sequence.elements.push_back(std::move(first_element.value()));
            while (auto separator = context.try_extract(separator_type)) {
                sequence.separator_tokens.push_back(context.token(separator.value()));
                sequence.elements.push_back(require<parser>(context, description.view()));
            }
        }
        return sequence;
    }

    template <parser auto parser, utl::Metastring description, Token_type separator>
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
        = extract_separated_zero_or_more<parser, description, Token_type::comma>;
    template <parser auto parser, utl::Metastring description>
    constexpr auto parse_comma_separated_one_or_more
        = parse_separated_one_or_more<parser, description, Token_type::comma>;

    auto name_from_token(Token const& token) -> kieli::Name;

    template <Token_type type, std::derived_from<kieli::Name> Name>
    auto parse_name(Context& context) -> std::optional<Name>
    {
        return context.try_extract(type).transform(
            [](Token const& token) { return Name { name_from_token(token) }; });
    }

    template <Token_type type, std::derived_from<kieli::Name> Name>
    auto extract_name(Context& context, std::string_view const description) -> Name
    {
        if (auto name = parse_name<type, Name>(context)) {
            return name.value();
        }
        context.error_expected(description);
    }

    constexpr auto parse_lower_name   = parse_name<Token_type::lower_name, kieli::Lower>;
    constexpr auto parse_upper_name   = parse_name<Token_type::upper_name, kieli::Upper>;
    constexpr auto extract_lower_name = extract_name<Token_type::lower_name, kieli::Lower>;
    constexpr auto extract_upper_name = extract_name<Token_type::upper_name, kieli::Upper>;

} // namespace libparse
