#ifndef KIELI_LIBCOMPILER_LSP
#define KIELI_LIBCOMPILER_LSP

#include <libutl/utilities.hpp>
#include <libcompiler/fwd.hpp>

namespace ki::lsp {

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
    struct Position {
        std::uint32_t line {};
        std::uint32_t column {};

        auto operator==(Position const&) const -> bool                  = default;
        auto operator<=>(Position const&) const -> std::strong_ordering = default;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
    struct Range {
        Position start; // Inclusive
        Position stop;  // Exclusive

        // Deliberately non-aggregate.
        explicit Range(Position start, Position stop);

        auto operator==(Range const&) const -> bool                  = default;
        auto operator<=>(Range const&) const -> std::strong_ordering = default;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#location
    struct Location {
        db::Document_id doc_id;
        Range           range;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
    enum struct Severity : std::uint8_t { Error, Warning, Hint, Information };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticRelatedInformation
    struct Diagnostic_related {
        std::string message;
        Location    location;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticTag
    enum struct Diagnostic_tag : std::uint8_t { None, Unnecessary, Deprecated };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
    struct Diagnostic {
        std::string                     message;
        Range                           range;
        Severity                        severity {};
        std::vector<Diagnostic_related> related_info;
        Diagnostic_tag                  tag {};
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#semanticTokenTypes
    enum struct Semantic_token_type : std::uint8_t {
        Comment,
        Constructor,
        Enumeration,
        Function,
        Interface,
        Keyword,
        Method,
        Module,
        Number,
        Operator_name,
        Parameter,
        Property,
        String,
        Structure,
        Type,
        Type_parameter,
        Variable,
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens
    struct Semantic_token {
        Position            position;
        std::uint32_t       length {};
        Semantic_token_type type {};
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentHighlightKind
    enum struct Reference_kind : std::uint8_t { Text, Read, Write };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentHighlight
    struct Reference {
        Range          range;
        Reference_kind kind {};
    };

    // Advance `position` with `character`.
    [[nodiscard]] auto advance(Position position, char character) noexcept -> Position;

    // Increase `position.column` by `offset`.
    [[nodiscard]] auto column_offset(Position position, std::uint32_t offset) noexcept -> Position;

    // Create a one-character range for `position`.
    [[nodiscard]] auto to_range(Position position) noexcept -> Range;

    // Create a zero-width range for `position`.
    [[nodiscard]] auto to_range_0(Position position) noexcept -> Range;

    // Check whether `position` is contained within `range`, excluding the end.
    [[nodiscard]] auto range_contains(Range range, Position position) noexcept -> bool;

    // Check whether `position` is contained within `range`, including the end.
    [[nodiscard]] auto range_contains_inclusive(Range range, Position position) noexcept -> bool;

    // Check whether `range` occupies more than one line.
    [[nodiscard]] auto is_multiline(Range range) noexcept -> bool;

    // Construct an error diagnostic.
    [[nodiscard]] auto error(Range range, std::string message) -> Diagnostic;

    // Construct a warning diagnostic.
    [[nodiscard]] auto warning(Range range, std::string message) -> Diagnostic;

    // Construct a note diagnostic.
    [[nodiscard]] auto note(Range range, std::string message) -> Diagnostic;

    // Capitalized severity description.
    [[nodiscard]] auto severity_string(Severity severity) -> std::string_view;

    // Construct a read reference.
    [[nodiscard]] auto read(Range range) noexcept -> Reference;

    // Construct a write reference.
    [[nodiscard]] auto write(Range range) noexcept -> Reference;

} // namespace ki::lsp

template <>
struct std::formatter<ki::lsp::Position> {
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(ki::lsp::Position const position, auto& ctx)
    {
        return std::format_to(ctx.out(), "{}:{}", position.line + 1, position.column + 1);
    }
};

template <>
struct std::formatter<ki::lsp::Range> {
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(ki::lsp::Range const& range, auto& ctx)
    {
        return std::format_to(ctx.out(), "{}-{}", range.start, range.stop);
    }
};

#endif // KIELI_LIBCOMPILER_LSP
