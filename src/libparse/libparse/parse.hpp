#ifndef KIELI_LIBPARSE_PARSE
#define KIELI_LIBPARSE_PARSE

#include <libcompiler/db.hpp>
#include <liblex/lex.hpp>

namespace ki::par {

    struct Failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    struct Context {
        db::Database&                    db;
        db::Document_id                  doc_id;
        cst::Arena                       arena;
        lex::State                       lex_state;
        std::optional<lex::Token>        next_token;
        std::optional<lsp::Position>     previous_token_end;
        std::vector<lsp::Semantic_token> semantic_tokens;
        std::size_t                      previous_path_semantic_offset {};
        std::size_t                      block_depth {};
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

    // Skip every token up to the next potential recovery point.
    void skip_to_next_recovery_point(Context& ctx);

    // Ensure module and impl blocks have been closed correctly.
    void handle_end_of_input(Context& ctx, lex::Token const& end);

    // Handle an unexpected token where a definition was expected.
    void handle_bad_token(Context& ctx, lex::Token const& token);

    auto extract_function(Context& ctx, lex::Token const& fn_keyword) -> cst::Function;
    auto extract_structure(Context& ctx, lex::Token const& struct_keyword) -> cst::Struct;
    auto extract_enumeration(Context& ctx, lex::Token const& enum_keyword) -> cst::Enum;
    auto extract_concept(Context& ctx, lex::Token const& concept_keyword) -> cst::Concept;
    auto extract_alias(Context& ctx, lex::Token const& alias_keyword) -> cst::Alias;
    auto extract_implementation(Context& ctx, lex::Token const& impl_keyword) -> cst::Impl_begin;
    auto extract_submodule(Context& ctx, lex::Token const& module_keyword) -> cst::Submodule_begin;
    auto extract_block_end(Context& ctx, lex::Token const& brace_close) -> cst::Block_end;

    void parse(Context& ctx, auto&& visitor)
    {
        for (;;) {
            try {
                switch (auto token = extract(ctx); token.type) {
                case lex::Type::Fn:           visitor(extract_function(ctx, token)); break;
                case lex::Type::Struct:       visitor(extract_structure(ctx, token)); break;
                case lex::Type::Enum:         visitor(extract_enumeration(ctx, token)); break;
                case lex::Type::Concept:      visitor(extract_concept(ctx, token)); break;
                case lex::Type::Alias:        visitor(extract_alias(ctx, token)); break;
                case lex::Type::Impl:         visitor(extract_implementation(ctx, token)); break;
                case lex::Type::Module:       visitor(extract_submodule(ctx, token)); break;
                case lex::Type::Brace_close:  visitor(extract_block_end(ctx, token)); break;
                case lex::Type::End_of_input: handle_end_of_input(ctx, token); return;
                default:                      handle_bad_token(ctx, token); break;
                }
            }
            catch (Failure const&) {
                skip_to_next_recovery_point(ctx);
            }
        }
    }

} // namespace ki::par

#endif // KIELI_LIBPARSE_PARSE
