#pragma once

#include <liblex/lex.hpp>
#include <libparse/cst.hpp>
#include <libutl/common/utilities.hpp>

namespace libparse {

    using kieli::Lexical_token;
    using Token_type = Lexical_token::Type;

    [[nodiscard]] auto is_name_token_type(Token_type) noexcept -> bool;

    [[nodiscard]] auto optional_token(Lexical_token const*) -> std::optional<cst::Token>;

    struct [[nodiscard]] Context {
        kieli::Compilation_info    compilation_info;
        cst::Node_arena            node_arena;
        std::vector<Lexical_token> tokens;
        Lexical_token*             start {};
        Lexical_token*             pointer {};
        utl::Pooled_string         plus_id;
        utl::Pooled_string         asterisk_id;

        explicit Context(kieli::Lex_result&&, cst::Node_arena&&) noexcept;

        [[nodiscard]] auto is_finished() const noexcept -> bool;
        [[nodiscard]] auto previous() const noexcept -> Lexical_token const&;
        [[nodiscard]] auto extract() noexcept -> Lexical_token const&;
        [[nodiscard]] auto try_extract(Token_type) noexcept -> Lexical_token const*;
        [[nodiscard]] auto extract_required(Token_type) -> Lexical_token const*;
        [[nodiscard]] auto try_consume(Token_type) noexcept -> bool;

        auto consume_required(Token_type) -> void;
        auto retreat() noexcept -> void;

        template <cst::node Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node>
        {
            return node_arena.wrap<Node>(std::move(node)); // NOLINT: move is correct
        }

        [[nodiscard]] auto wrap() noexcept
        {
            return [this]<cst::node Node>(Node&& node) -> utl::Wrapper<Node> {
                return wrap(std::move(node)); // NOLINT: move is correct
            };
        }

        [[noreturn]] auto error_expected(
            utl::Source_view erroneous_view, std::string_view expectation) -> void;

        [[noreturn]] auto error_expected(std::string_view expectation) -> void;

        [[nodiscard]] auto diagnostics() noexcept -> kieli::Diagnostics&;

        auto make_source_view(Lexical_token const*, Lexical_token const*) noexcept
            -> utl::Source_view;
    };

    template <class Parse>
    concept parser = requires(Parse parse, Context context) {
        // clang-format off
        { parse(context) } -> utl::specialization_of<std::optional>;
        // clang-format on
    };

    template <parser auto p>
    using Parse_result = typename std::invoke_result_t<decltype(p), Context&>::value_type;

    template <parser auto p, utl::Metastring description>
    auto extract_required(Context& context) -> Parse_result<p>
    {
        if (auto result = p(context)) {
            return std::move(*result);
        }
        context.error_expected(description.view());
    }

    template <parser auto p, parser auto... ps>
    auto parse_one_of(Context& context) -> decltype(p(context))
    {
        if (auto result = p(context)) {
            return result;
        }
        if constexpr (sizeof...(ps) != 0) {
            return parse_one_of<ps...>(context);
        }
        else {
            return std::nullopt;
        }
    }

    template <parser auto p, utl::Metastring description, Token_type open, Token_type close>
    auto parse_surrounded(Context& context) -> std::optional<cst::Surrounded<Parse_result<p>>>
    {
        if (Lexical_token const* const open_token = context.try_extract(open)) {
            if (auto result = p(context)) {
                if (Lexical_token const* const close_token = context.try_extract(close)) {
                    return cst::Surrounded<Parse_result<p>> {
                        .value       = std::move(*result),
                        .open_token  = cst::Token::from_lexical(open_token),
                        .close_token = cst::Token::from_lexical(close_token),
                    };
                }
                context.error_expected("a closing '{}'"_format(close));
            }
            context.error_expected(description.view());
        }
        return std::nullopt;
    }

    template <parser auto p, utl::Metastring description>
    constexpr auto parenthesized
        = parse_surrounded<p, description, Token_type::paren_open, Token_type::paren_close>;
    template <parser auto p, utl::Metastring description>
    constexpr auto braced
        = parse_surrounded<p, description, Token_type::brace_open, Token_type::brace_close>;
    template <parser auto p, utl::Metastring description>
    constexpr auto bracketed
        = parse_surrounded<p, description, Token_type::bracket_close, Token_type::bracket_close>;

    template <parser auto p, Token_type separator, utl::Metastring description>
    auto extract_separated_zero_or_more(Context& context)
        -> cst::Separated_sequence<Parse_result<p>>
    {
        cst::Separated_sequence<Parse_result<p>> sequence;
        while (auto value = p(context)) {
            sequence.elements.push_back(std::move(*value));
            if (Lexical_token const* const separator_token = context.try_extract(separator)) {
                sequence.separator_tokens.push_back(cst::Token::from_lexical(separator_token));
            }
            else {
                return sequence;
            }
        }
        if (sequence.elements.empty()) {
            return sequence;
        }
        context.error_expected(description.view());
    }

    template <parser auto p, Token_type separator, utl::Metastring description>
    auto parse_separated_one_or_more(Context& context)
        -> std::optional<cst::Separated_sequence<Parse_result<p>>>
    {
        auto sequence = extract_separated_zero_or_more<p, separator, description>(context);
        if (sequence.elements.empty()) {
            return std::nullopt;
        }
        return sequence;
    }

    template <parser auto p, utl::Metastring description>
    constexpr auto extract_comma_separated_zero_or_more
        = extract_separated_zero_or_more<p, Token_type::comma, description>;
    template <parser auto p, utl::Metastring description>
    constexpr auto parse_comma_separated_one_or_more
        = parse_separated_one_or_more<p, Token_type::comma, description>;

    auto parse_expression(Context&) -> std::optional<utl::Wrapper<cst::Expression>>;
    auto parse_pattern(Context&) -> std::optional<utl::Wrapper<cst::Pattern>>;
    auto parse_type(Context&) -> std::optional<utl::Wrapper<cst::Type>>;

    constexpr auto extract_expression = extract_required<parse_expression, "an expression">;
    constexpr auto extract_pattern    = extract_required<parse_pattern, "a pattern">;
    constexpr auto extract_type       = extract_required<parse_type, "a type">;

    auto parse_top_level_pattern(Context&) -> std::optional<utl::Wrapper<cst::Pattern>>;
    auto parse_block_expression(Context&) -> std::optional<utl::Wrapper<cst::Expression>>;
    auto parse_template_arguments(Context&) -> std::optional<cst::Template_arguments>;
    auto parse_template_parameters(Context&) -> std::optional<cst::Template_parameters>;
    auto parse_class_reference(Context&) -> std::optional<cst::Class_reference>;
    auto parse_mutability(Context&) -> std::optional<cst::Mutability>;
    auto parse_type_annotation(Context&) -> std::optional<cst::Type_annotation>;

    auto extract_function_parameters(Context&) -> cst::Separated_sequence<cst::Function_parameter>;
    auto extract_class_references(Context&) -> cst::Separated_sequence<cst::Class_reference>;

    auto extract_qualified(Context&, std::optional<cst::Root_qualifier>&&) -> cst::Qualified_name;

    template <Token_type identifier_type>
    auto extract_id(Context& context, std::string_view const description) -> utl::Pooled_string
    {
        if (Lexical_token const* const token = context.try_extract(identifier_type)) {
            return token->as_string();
        }
        context.error_expected(description);
    }

    constexpr auto extract_lower_id = extract_id<Token_type::lower_name>;
    constexpr auto extract_upper_id = extract_id<Token_type::upper_name>;

    template <Token_type identifier_type>
    auto parse_id(Context& context) -> std::optional<utl::Pooled_string>
    {
        if (Lexical_token const* const token = context.try_extract(identifier_type)) {
            return token->as_string();
        }
        return std::nullopt;
    }

    constexpr auto parse_lower_id = parse_id<Token_type::lower_name>;
    constexpr auto parse_upper_id = parse_id<Token_type::upper_name>;

    template <Token_type identifier_type, class Name>
    auto parse_name(Context& context) -> std::optional<Name>
    {
        if (Lexical_token const* const token = context.try_extract(identifier_type)) {
            return Name {
                .identifier  = token->as_string(),
                .source_view = token->source_view,
            };
        }
        return std::nullopt;
    }

    constexpr auto parse_lower_name = parse_name<Token_type::lower_name, kieli::Name_lower>;
    constexpr auto parse_upper_name = parse_name<Token_type::upper_name, kieli::Name_upper>;

    template <Token_type identifier_type, class Name>
    auto extract_name(Context& context, std::string_view const description) -> Name
    {
        if (auto name = parse_name<identifier_type, Name>(context)) {
            return std::move(*name);
        }
        context.error_expected(description);
    }

    constexpr auto extract_lower_name = extract_name<Token_type::lower_name, kieli::Name_lower>;
    constexpr auto extract_upper_name = extract_name<Token_type::upper_name, kieli::Name_upper>;

    template <class Node, parser auto parse>
    auto parse_node(Context& context) -> std::optional<utl::Wrapper<Node>>
    {
        Lexical_token const* const anchor = context.pointer;
        if (auto node_value = parse(context)) {
            return context.wrap(Node {
                std::move(*node_value),
                context.make_source_view(anchor, context.pointer - 1),
            });
        }
        return std::nullopt;
    }

} // namespace libparse
