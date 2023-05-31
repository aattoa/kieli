#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/diagnostics/diagnostics.hpp>
#include <liblex/lex.hpp>
#include <libparse/ast.hpp>


namespace libparse {

    using Token = kieli::Lexical_token;

    auto make_source_view(Token const*, Token const*) noexcept -> utl::Source_view;


    struct [[nodiscard]] Parse_context {
        compiler::Compilation_info compilation_info;
        ast::Node_arena            node_arena;
        std::vector<Token>         tokens;
        Token*                     start;
        Token*                     pointer;

        compiler::Operator plus_id;
        compiler::Operator asterisk_id;

        explicit Parse_context(kieli::Lex_result&&, ast::Node_arena&&) noexcept;

        [[nodiscard]] auto is_finished()      const noexcept -> bool;
        [[nodiscard]] auto previous()         const noexcept -> Token const&;
        [[nodiscard]] auto extract()                noexcept -> Token const&;
        [[nodiscard]] auto try_extract(Token::Type) noexcept -> Token const*;
        [[nodiscard]] auto try_consume(Token::Type) noexcept -> bool;

        auto consume_required(Token::Type) -> void;
        auto retreat() noexcept -> void;

        template <ast::node Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node> {
            return node_arena.wrap(std::move(node));
        }
        [[nodiscard]]
        auto wrap() noexcept {
            return [this]<ast::node Node>(Node&& node) -> utl::Wrapper<Node> {
                return wrap(std::move(node));
            };
        }

        [[nodiscard]] auto diagnostics() noexcept -> utl::diagnostics::Builder&;

        [[noreturn]] auto error(utl::Source_view, utl::diagnostics::Message_arguments const&) -> void;
        [[noreturn]] auto error(utl::diagnostics::Message_arguments const&) -> void;
        [[noreturn]] auto error_expected(utl::Source_view, std::string_view expectation, tl::optional<std::string_view> help = tl::nullopt) -> void;
        [[noreturn]] auto error_expected(std::string_view expectation, tl::optional<std::string_view> help = tl::nullopt) -> void;
    };


    template <class P>
    concept parser = requires (P p, Parse_context context) {
        { p(context) } -> utl::instance_of<tl::optional>;
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

        if constexpr (sizeof...(ps) != 0)
            return parse_one_of<ps...>(context);
        else
            return tl::nullopt;
    }


    template <parser auto p, utl::Metastring description, Token::Type open, Token::Type close>
    auto parse_surrounded(Parse_context& context) -> decltype(p(context)) {
        if (context.try_consume(open)) {
            if (auto result = p(context)) {
                if (context.try_consume(close))
                    return result;
                else
                    context.error_expected("a closing '{}'"_format(close));
            }
            context.error_expected(description.view());
        }
        return tl::nullopt;
    }


    template <parser auto p, utl::Metastring description>
    constexpr auto parenthesized = parse_surrounded<p, description, Token::Type::paren_open, Token::Type::paren_close>;
    template <parser auto p, utl::Metastring description>
    constexpr auto braced = parse_surrounded<p, description, Token::Type::brace_open, Token::Type::brace_close>;
    template <parser auto p, utl::Metastring description>
    constexpr auto bracketed = parse_surrounded<p, description, Token::Type::bracket_close, Token::Type::bracket_close>;


    template <parser auto p, Token::Type separator, utl::Metastring description>
    auto extract_separated_zero_or_more(Parse_context& context) -> std::vector<Parse_result<p>> {
        std::vector<Parse_result<p>> vector;

        if (auto head = p(context)) {
            vector.push_back(std::move(*head));

            while (context.try_consume(separator)) {
                if (auto element = p(context))
                    vector.push_back(std::move(*element));
                else
                    context.error_expected(description.view());
            }
        }

        return vector;
    }

    template <parser auto p, Token::Type separator, utl::Metastring description>
    auto parse_separated_one_or_more(Parse_context& context) -> tl::optional<std::vector<Parse_result<p>>> {
        auto vector = extract_separated_zero_or_more<p, separator, description>(context);
        if (!vector.empty())
            return vector;
        else
            return tl::nullopt;
    }


    template <parser auto p, utl::Metastring description>
    constexpr auto extract_comma_separated_zero_or_more = extract_separated_zero_or_more<p, Token::Type::comma, description>;
    template <parser auto p, utl::Metastring description>
    constexpr auto parse_comma_separated_one_or_more = parse_separated_one_or_more<p, Token::Type::comma, description>;


    auto parse_expression(Parse_context&) -> tl::optional<ast::Expression>;
    auto parse_pattern   (Parse_context&) -> tl::optional<ast::Pattern   >;
    auto parse_type      (Parse_context&) -> tl::optional<ast::Type      >;

    constexpr auto extract_expression = extract_required<parse_expression, "an expression">;
    constexpr auto extract_pattern    = extract_required<parse_pattern,    "a pattern"    >;
    constexpr auto extract_type       = extract_required<parse_type,       "a type"       >;

    constexpr auto extract_type_sequence = extract_comma_separated_zero_or_more<parse_type, "a type">;

    auto parse_top_level_pattern  (Parse_context&) -> tl::optional<ast::Pattern>;
    auto parse_block_expression   (Parse_context&) -> tl::optional<ast::Expression>;
    auto parse_template_arguments (Parse_context&) -> tl::optional<std::vector<ast::Template_argument>>;
    auto parse_template_parameters(Parse_context&) -> tl::optional<std::vector<ast::Template_parameter>>;
    auto parse_class_reference    (Parse_context&) -> tl::optional<ast::Class_reference>;

    auto extract_function_parameters             (Parse_context&) -> std::vector<ast::Function_parameter>;
    auto extract_qualified(ast::Root_qualifier&&, Parse_context&) -> ast::Qualified_name;
    auto extract_mutability                      (Parse_context&) -> ast::Mutability;
    auto extract_class_references                (Parse_context&) -> std::vector<ast::Class_reference>;


    template <Token::Type id_type>
    auto extract_id(Parse_context& context, std::string_view const description) -> compiler::Identifier {
        if (Token const* const token = context.try_extract(id_type))
            return token->as_identifier();
        else
            context.error_expected(description);
    }

    constexpr auto extract_lower_id = extract_id<Token::Type::lower_name>;
    constexpr auto extract_upper_id = extract_id<Token::Type::upper_name>;


    template <Token::Type id_type>
    auto parse_id(Parse_context& context) -> tl::optional<compiler::Identifier> {
        if (Token const* const token = context.try_extract(id_type))
            return token->as_identifier();
        else
            return tl::nullopt;
    }

    constexpr auto parse_lower_id = parse_id<Token::Type::lower_name>;
    constexpr auto parse_upper_id = parse_id<Token::Type::upper_name>;


    template <Token::Type id_type>
    auto parse_name(Parse_context& context) -> tl::optional<ast::Name> {
        if (Token const* const token = context.try_extract(id_type)) {
            return ast::Name {
                .identifier  = token->as_identifier(),
                .is_upper    = id_type == Token::Type::upper_name,
                .source_view = token->source_view,
            };
        }
        return tl::nullopt;
    }

    constexpr auto parse_lower_name = parse_name<Token::Type::lower_name>;
    constexpr auto parse_upper_name = parse_name<Token::Type::upper_name>;


    template <Token::Type id_type>
    auto extract_name(Parse_context& context, std::string_view const description) -> ast::Name {
        if (auto name = parse_name<id_type>(context))
            return std::move(*name);
        else
            context.error_expected(description);
    }

    constexpr auto extract_lower_name = extract_name<Token::Type::lower_name>;
    constexpr auto extract_upper_name = extract_name<Token::Type::upper_name>;


    template <class Node, parser auto parse>
    auto parse_node(Parse_context& context) -> tl::optional<Node> {
        Token const* const anchor = context.pointer;

        if (auto node_value = parse(context))
            return Node { std::move(*node_value), make_source_view(anchor, context.pointer -1) };
        else
            return tl::nullopt;
    }

}
