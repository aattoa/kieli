#ifndef KIELI_LIBFORMAT_FORMAT
#define KIELI_LIBFORMAT_FORMAT

#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>

namespace ki::fmt {

    enum struct Function_body : std::uint8_t {
        Leave_as_is,
        Normalize_to_block,
        Normalize_to_equals_sign
    };

    struct Options {
        std::size_t   tab_size                        = 4;
        bool          use_spaces                      = true;
        std::size_t   empty_lines_between_definitions = 1;
        Function_body function_body                   = Function_body::Leave_as_is;
    };

    struct Context {
        db::Database const& db;
        cst::Arena const&   arena;
        std::ostream&       stream;
        Options const&      options {};
        std::size_t         indentation {};
        bool                did_open_block {};
        bool                is_first_definition = true;
    };

    // Parse and format the given document.
    auto format_document(
        std::ostream&       stream,
        db::Database&       db,
        db::Document_id     doc_id,
        db::Diagnostic_sink sink,
        Options const&      options) -> lsp::Range;

    void format(Context& ctx, cst::Function const& function);
    void format(Context& ctx, cst::Struct const& structure);
    void format(Context& ctx, cst::Enum const& enumeration);
    void format(Context& ctx, cst::Alias const& alias);
    void format(Context& ctx, cst::Concept const& concept_);
    void format(Context& ctx, cst::Impl_begin const& impl);
    void format(Context& ctx, cst::Submodule_begin const& submodule);
    void format(Context& ctx, cst::Block_end const& block_end);

    void format(Context& ctx, cst::Expression const& expression);
    void format(Context& ctx, cst::Expression_id expression_id);
    void format(Context& ctx, cst::Pattern const& pattern);
    void format(Context& ctx, cst::Pattern_id pattern_id);
    void format(Context& ctx, cst::Type const& type);
    void format(Context& ctx, cst::Type_id type_id);

    auto to_string(db::Database const& db, cst::Arena const& arena, auto const& x) -> std::string
    {
        auto out = std::ostringstream {};
        auto ctx = Context { .db = db, .arena = arena, .stream = out };
        ki::fmt::format(ctx, x);
        return std::move(out).str();
    }

} // namespace ki::fmt

#endif // KIELI_LIBFORMAT_FORMAT
