#ifndef KIELI_LIBPARSE_INTERNALS
#define KIELI_LIBPARSE_INTERNALS

#include <liblex/lex.hpp>
#include <libcompiler/db.hpp>

namespace ki::par {

    using Semantic = lsp::Semantic_token_type;

    struct Failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    struct Context {
        db::Database&                    db;
        cst::Arena                       arena;
        db::Document_id                  doc_id;
        lex::State                       lex_state;
        std::optional<lex::Token>        next_token;
        std::optional<lsp::Position>     previous_token_end;
        std::vector<lsp::Semantic_token> semantic_tokens;
        std::size_t                      previous_path_semantic_offset {};
        utl::String_id                   plus_id;
        utl::String_id                   asterisk_id;
    };

    // Create a parse context.
    [[nodiscard]] auto context(db::Database& db, db::Document_id doc_id) -> Context;

    // Check whether the current token is the end-of-input token.
    [[nodiscard]] auto is_finished(Context& ctx) -> bool;

    // Inspect the current token without consuming it.
    [[nodiscard]] auto peek(Context& ctx) -> lex::Token;

    // Consume the current token.
    [[nodiscard]] auto extract(Context& ctx) -> lex::Token;

    // Consume the current token if it matches `type`.
    [[nodiscard]] auto try_extract(Context& ctx, lex::Type type) -> std::optional<lex::Token>;

    // Consume the current token if it matches `type`, otherwise emit an error.
    [[nodiscard]] auto require_extract(Context& ctx, lex::Type type) -> lex::Token;

    // Source range from `range` up to (but not including) the current token.
    [[nodiscard]] auto up_to_current(Context& ctx, lsp::Range range) -> cst::Range_id;

    // Add a token range to the CST arena, and return its id.
    [[nodiscard]] auto token(Context& ctx, lex::Token const& token) -> cst::Range_id;

    // Add a semantic token corresponding to `range` to the current document.
    void add_semantic_token(Context& ctx, lsp::Range range, Semantic type);

    // Add a keyword semantic token corresponding to `range` to the current document.
    void add_keyword(Context& ctx, lsp::Range range);

    // Add a punctuation semantic token corresponding to `token` to the current document.
    void add_punctuation(Context& ctx, lsp::Range range);

    // Set the previously parsed path head's semantic type to `type`.
    void set_previous_path_head_semantic_type(Context& ctx, Semantic type);

    // Emit an error that describes an expectation failure:
    // Encountered `error_range` where `description` was expected.
    [[noreturn]] void error_expected(Context& ctx, lsp::Range range, std::string_view description);

    // Emit an error that describes an expectation failure:
    // Encountered the current token where `description` was expected.
    [[noreturn]] void error_expected(Context& ctx, std::string_view description);

    // Create an identifier from a token.
    [[nodiscard]] auto identifier(Context& ctx, lex::Token const& token) -> utl::String_id;

    // Create a name from a token.
    [[nodiscard]] auto name(Context& ctx, lex::Token const& token) -> db::Name;

    // Check whether `type` is a recovery point.
    [[nodiscard]] auto is_recovery_point(lex::Type type) -> bool;

    // Skip every token up to the next potential recovery point.
    void skip_to_next_recovery_point(Context& ctx);

    auto parse_simple_path_root(Context& ctx) -> std::optional<cst::Path_root>;
    auto parse_simple_path(Context& ctx) -> std::optional<cst::Path>;
    auto parse_complex_path(Context& ctx) -> std::optional<cst::Path>;
    auto extract_path(Context& ctx, cst::Path_root) -> cst::Path;
    auto extract_concept_references(Context& ctx) -> cst::Separated<cst::Path>;

    auto parse_block_expression(Context& ctx) -> std::optional<cst::Expression_id>;
    auto parse_expression(Context& ctx) -> std::optional<cst::Expression_id>;

    auto parse_top_level_pattern(Context& ctx) -> std::optional<cst::Pattern_id>;
    auto parse_pattern(Context& ctx) -> std::optional<cst::Pattern_id>;

    auto parse_template_parameters(Context& ctx) -> std::optional<cst::Template_parameters>;
    auto parse_template_parameter(Context& ctx) -> std::optional<cst::Template_parameter>;
    auto parse_template_arguments(Context& ctx) -> std::optional<cst::Template_arguments>;
    auto parse_template_argument(Context& ctx) -> std::optional<cst::Template_argument>;

    auto parse_function_parameters(Context& ctx) -> std::optional<cst::Function_parameters>;
    auto parse_function_parameter(Context& ctx) -> std::optional<cst::Function_parameter>;
    auto parse_function_arguments(Context& ctx) -> std::optional<cst::Function_arguments>;

    auto parse_definition(Context& ctx) -> std::optional<cst::Definition>;
    auto parse_mutability(Context& ctx) -> std::optional<cst::Mutability>;
    auto parse_type_annotation(Context& ctx) -> std::optional<cst::Type_annotation>;
    auto parse_type_root(Context& ctx) -> std::optional<cst::Type_id>;
    auto parse_type(Context& ctx) -> std::optional<cst::Type_id>;

    template <std::invocable<Context&> auto extract>
    auto pretend_parse(Context& ctx) -> std::optional<decltype(extract(ctx))>
    {
        return extract(ctx);
    }

    template <std::invocable<Context&> auto parser>
    auto require(Context& ctx, std::string_view const description)
        -> decltype(parser(ctx))::value_type
    {
        if (auto result = parser(ctx)) {
            return std::move(result).value();
        }
        error_expected(ctx, description);
    }

    template <std::invocable<Context&> auto parser, utl::Metastring description>
    auto require(Context& ctx) -> decltype(parser(ctx))::value_type
    {
        return require<parser>(ctx, description.view());
    }

    template <
        std::invocable<Context&> auto parser,
        utl::Metastring               description,
        lex::Type                     open_type,
        lex::Type                     close_type>
    auto parse_surrounded(Context& ctx)
        -> std::optional<cst::Surrounded<typename decltype(parser(ctx))::value_type>>
    {
        return try_extract(ctx, open_type).transform([&](lex::Token const& open) {
            add_punctuation(ctx, open.range);
            auto value = require<parser>(ctx, description.view());
            auto close = require_extract(ctx, close_type);
            add_punctuation(ctx, close.range);
            return cst::Surrounded {
                .value       = std::move(value),
                .open_token  = token(ctx, open),
                .close_token = token(ctx, close),
            };
        });
    }

    template <std::invocable<Context&> auto parser, utl::Metastring description>
    constexpr auto parse_parenthesized
        = parse_surrounded<parser, description, lex::Type::Paren_open, lex::Type::Paren_close>;
    template <std::invocable<Context&> auto parser, utl::Metastring description>
    constexpr auto parse_braced
        = parse_surrounded<parser, description, lex::Type::Brace_open, lex::Type::Brace_close>;
    template <std::invocable<Context&> auto parser, utl::Metastring description>
    constexpr auto parse_bracketed
        = parse_surrounded<parser, description, lex::Type::Bracket_open, lex::Type::Bracket_close>;

    template <
        std::invocable<Context&> auto parser,
        utl::Metastring               description,
        lex::Type                     separator_type>
    auto extract_separated_zero_or_more(Context& ctx)
        -> cst::Separated<typename decltype(parser(ctx))::value_type>
    {
        cst::Separated<typename decltype(parser(ctx))::value_type> sequence;
        if (auto first_element = parser(ctx)) {
            sequence.elements.push_back(std::move(first_element).value());
            while (auto const separator = try_extract(ctx, separator_type)) {
                add_punctuation(ctx, separator.value().range);
                sequence.separator_tokens.push_back(token(ctx, separator.value()));
                sequence.elements.push_back(require<parser>(ctx, description.view()));
            }
        }
        return sequence;
    }

    template <
        std::invocable<Context&> auto parser,
        utl::Metastring               description,
        lex::Type                     separator>
    auto parse_separated_one_or_more(Context& ctx)
        -> std::optional<cst::Separated<typename decltype(parser(ctx))::value_type>>
    {
        auto sequence = extract_separated_zero_or_more<parser, description, separator>(ctx);
        if (sequence.elements.empty()) {
            return std::nullopt;
        }
        return sequence;
    }

    template <std::invocable<Context&> auto parser, utl::Metastring description>
    constexpr auto extract_comma_separated_zero_or_more
        = extract_separated_zero_or_more<parser, description, lex::Type::Comma>;
    template <std::invocable<Context&> auto parser, utl::Metastring description>
    constexpr auto parse_comma_separated_one_or_more
        = parse_separated_one_or_more<parser, description, lex::Type::Comma>;

    template <lex::Type type, std::derived_from<db::Name> Name>
    auto parse_name(Context& ctx) -> std::optional<Name>
    {
        return try_extract(ctx, type).transform(
            [&](lex::Token const& token) { return Name { name(ctx, token) }; });
    }

    template <lex::Type type, std::derived_from<db::Name> Name>
    auto extract_name(Context& ctx, std::string_view const description) -> Name
    {
        if (auto name = parse_name<type, Name>(ctx)) {
            return name.value();
        }
        error_expected(ctx, description);
    }

    constexpr auto parse_lower_name   = parse_name<lex::Type::Lower_name, db::Lower>;
    constexpr auto parse_upper_name   = parse_name<lex::Type::Upper_name, db::Upper>;
    constexpr auto extract_lower_name = extract_name<lex::Type::Lower_name, db::Lower>;
    constexpr auto extract_upper_name = extract_name<lex::Type::Upper_name, db::Upper>;

    auto parse_string(Context& ctx, lex::Token const& literal) -> std::optional<db::String>;
    auto parse_integer(Context& ctx, lex::Token const& literal) -> std::optional<db::Integer>;
    auto parse_floating(Context& ctx, lex::Token const& literal) -> std::optional<db::Floating>;
    auto parse_boolean(Context& ctx, lex::Token const& literal) -> std::optional<db::Boolean>;

} // namespace ki::par

#endif // KIELI_LIBPARSE_INTERNALS
