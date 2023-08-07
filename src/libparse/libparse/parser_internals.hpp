#pragma once

#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include <libparse/cst.hpp>


namespace libparse {

    using kieli::Lexical_token;
    using Token_type = Lexical_token::Type;

    [[nodiscard]]
    auto is_name_token_type(Token_type) noexcept -> bool;

    [[nodiscard]]
    auto optional_token(Lexical_token const*) -> tl::optional<cst::Token>;


    struct [[nodiscard]] Parse_context {
        compiler::Compilation_info compilation_info;
        cst::Node_arena            node_arena;
        std::vector<Lexical_token> tokens;
        Lexical_token*             start;
        Lexical_token*             pointer;
        utl::Pooled_string         plus_id;
        utl::Pooled_string         asterisk_id;

        explicit Parse_context(kieli::Lex_result&&, cst::Node_arena&&) noexcept;

        [[nodiscard]] auto is_finished()          const noexcept -> bool;
        [[nodiscard]] auto previous()             const noexcept -> Lexical_token const&;
        [[nodiscard]] auto extract()                    noexcept -> Lexical_token const&;
        [[nodiscard]] auto try_extract     (Token_type) noexcept -> Lexical_token const*;
        [[nodiscard]] auto extract_required(Token_type)          -> Lexical_token const*;
        [[nodiscard]] auto try_consume     (Token_type) noexcept -> bool;

        auto consume_required(Token_type) -> void;
        auto retreat() noexcept -> void;

        template <cst::node Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node> {
            return node_arena.wrap(std::move(node)); // NOLINT: std::move is correct due to constraint
        }
        [[nodiscard]]
        auto wrap() noexcept {
            return [this]<cst::node Node>(Node&& node) -> utl::Wrapper<Node> {
                return wrap(std::move(node)); // NOLINT: std::move is correct due to constraint
            };
        }

        [[nodiscard]] auto diagnostics() noexcept -> utl::diagnostics::Builder&;

        [[noreturn]] auto error(utl::Source_view, utl::diagnostics::Message_arguments const&) -> void;
        [[noreturn]] auto error(utl::diagnostics::Message_arguments const&) -> void;
        [[noreturn]] auto error_expected(utl::Source_view, std::string_view expectation, tl::optional<std::string_view> help = tl::nullopt) -> void;
        [[noreturn]] auto error_expected(std::string_view expectation, tl::optional<std::string_view> help = tl::nullopt) -> void;

        auto make_source_view(Lexical_token const*, Lexical_token const*) noexcept -> utl::Source_view;
    };


    template <class P>
    concept parser = requires (P p, Parse_context context) {
        { p(context) } -> utl::specialization_of<tl::optional>;
    };

    template <parser auto p>
    using Parse_result = typename std::invoke_result_t<decltype(p), Parse_context&>::value_type;


    template <parser auto p, utl::Metastring description>
    auto extract_required(Parse_context& context) -> Parse_result<p> {
        if (auto result = p(context))
            return std::move(*result);
        else
            context.error_expected(description.view());
    }

    template <parser auto p, parser auto... ps>
    auto parse_one_of(Parse_context& context) -> decltype(p(context)) {
        if (auto result = p(context))
            return result;
        else if constexpr (sizeof...(ps) != 0)
            return parse_one_of<ps...>(context);
        else
            return tl::nullopt;
    }


    template <parser auto p, utl::Metastring description, Token_type open, Token_type close>
    auto parse_surrounded(Parse_context& context)
        -> tl::optional<cst::Surrounded<Parse_result<p>>>
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
        return tl::nullopt;
    }


    template <parser auto p, utl::Metastring description>
    constexpr auto parenthesized = parse_surrounded<p, description, Token_type::paren_open, Token_type::paren_close>;
    template <parser auto p, utl::Metastring description>
    constexpr auto braced = parse_surrounded<p, description, Token_type::brace_open, Token_type::brace_close>;
    template <parser auto p, utl::Metastring description>
    constexpr auto bracketed = parse_surrounded<p, description, Token_type::bracket_close, Token_type::bracket_close>;


    template <parser auto p, Token_type separator, utl::Metastring description>
    auto extract_separated_zero_or_more(Parse_context& context)
        -> cst::Separated_sequence<Parse_result<p>>
    {
        cst::Separated_sequence<Parse_result<p>> sequence;
        while (auto value = p(context)) {
            sequence.elements.push_back(std::move(*value));
            if (Lexical_token const* const separator_token = context.try_extract(separator))
                sequence.separator_tokens.push_back(cst::Token::from_lexical(separator_token));
            else
                return sequence;
        }
        if (sequence.elements.empty())
            return sequence;
        else
            context.error_expected(description.view());
    }

    template <parser auto p, Token_type separator, utl::Metastring description>
    auto parse_separated_one_or_more(Parse_context& context)
        -> tl::optional<cst::Separated_sequence<Parse_result<p>>>
    {
        auto sequence = extract_separated_zero_or_more<p, separator, description>(context);
        if (sequence.elements.empty())
            return tl::nullopt;
        else
            return sequence;
    }


    template <parser auto p, utl::Metastring description>
    constexpr auto extract_comma_separated_zero_or_more = extract_separated_zero_or_more<p, Token_type::comma, description>;
    template <parser auto p, utl::Metastring description>
    constexpr auto parse_comma_separated_one_or_more = parse_separated_one_or_more<p, Token_type::comma, description>;


    auto parse_expression(Parse_context&) -> tl::optional<utl::Wrapper<cst::Expression>>;
    auto parse_pattern   (Parse_context&) -> tl::optional<utl::Wrapper<cst::Pattern>>;
    auto parse_type      (Parse_context&) -> tl::optional<utl::Wrapper<cst::Type>>;

    constexpr auto extract_expression = extract_required<parse_expression, "an expression">;
    constexpr auto extract_pattern    = extract_required<parse_pattern,    "a pattern">;
    constexpr auto extract_type       = extract_required<parse_type,       "a type">;

    auto parse_top_level_pattern  (Parse_context&) -> tl::optional<utl::Wrapper<cst::Pattern>>;
    auto parse_block_expression   (Parse_context&) -> tl::optional<utl::Wrapper<cst::Expression>>;
    auto parse_template_arguments (Parse_context&) -> tl::optional<cst::Template_arguments>;
    auto parse_template_parameters(Parse_context&) -> tl::optional<cst::Template_parameters>;
    auto parse_class_reference    (Parse_context&) -> tl::optional<cst::Class_reference>;
    auto parse_mutability         (Parse_context&) -> tl::optional<cst::Mutability>;
    auto parse_type_annotation    (Parse_context&) -> tl::optional<cst::Type_annotation>;

    auto extract_function_parameters(Parse_context&) -> cst::Separated_sequence<cst::Function_parameter>;
    auto extract_class_references   (Parse_context&) -> cst::Separated_sequence<cst::Class_reference>;

    auto extract_qualified(Parse_context&, tl::optional<cst::Root_qualifier>&&) -> cst::Qualified_name;


    template <Token_type identifier_type>
    auto extract_id(Parse_context& context, std::string_view const description) -> utl::Pooled_string {
        if (Lexical_token const* const token = context.try_extract(identifier_type))
            return token->as_string();
        else
            context.error_expected(description);
    }

    constexpr auto extract_lower_id = extract_id<Token_type::lower_name>;
    constexpr auto extract_upper_id = extract_id<Token_type::upper_name>;


    template <Token_type identifier_type>
    auto parse_id(Parse_context& context) -> tl::optional<utl::Pooled_string> {
        if (Lexical_token const* const token = context.try_extract(identifier_type))
            return token->as_string();
        else
            return tl::nullopt;
    }

    constexpr auto parse_lower_id = parse_id<Token_type::lower_name>;
    constexpr auto parse_upper_id = parse_id<Token_type::upper_name>;


    template <Token_type identifier_type, class Name>
    auto parse_name(Parse_context& context) -> tl::optional<Name> {
        if (Lexical_token const* const token = context.try_extract(identifier_type)) {
            return Name {
                .identifier  = token->as_string(),
                .source_view = token->source_view,
            };
        }
        return tl::nullopt;
    }

    constexpr auto parse_lower_name = parse_name<Token_type::lower_name, compiler::Name_lower>;
    constexpr auto parse_upper_name = parse_name<Token_type::upper_name, compiler::Name_upper>;


    template <Token_type identifier_type, class Name>
    auto extract_name(Parse_context& context, std::string_view const description) -> Name {
        if (auto name = parse_name<identifier_type, Name>(context))
            return std::move(*name);
        else
            context.error_expected(description);
    }

    constexpr auto extract_lower_name = extract_name<Token_type::lower_name, compiler::Name_lower>;
    constexpr auto extract_upper_name = extract_name<Token_type::upper_name, compiler::Name_upper>;


    template <class Node, parser auto parse>
    auto parse_node(Parse_context& context) -> tl::optional<utl::Wrapper<Node>> {
        Lexical_token const* const anchor = context.pointer;
        if (auto node_value = parse(context))
            return context.wrap(Node { std::move(*node_value), context.make_source_view(anchor, context.pointer - 1) });
        else
            return tl::nullopt;
    }

}
