#ifndef KIELI_LIBCOMPILER_COMPILER
#define KIELI_LIBCOMPILER_COMPILER

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/string_pool.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace ki {

    // Identifies a document.
    struct Document_id : utl::Vector_index<Document_id> {
        using Vector_index::Vector_index;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
    struct Position {
        std::uint32_t line {};
        std::uint32_t column {};

        // Advance this position with `character`.
        void advance_with(char character) noexcept;

        // Create a position with `column` increased by `offset`.
        [[nodiscard]] auto horizontal_offset(std::uint32_t offset) const noexcept -> Position;

        [[nodiscard]] auto operator==(Position const&) const -> bool                  = default;
        [[nodiscard]] auto operator<=>(Position const&) const -> std::strong_ordering = default;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
    struct Range {
        Position start; // Inclusive
        Position stop;  // Exclusive

        // Deliberately non-aggregate.
        explicit Range(Position start, Position stop) noexcept;

        // Create a one-character range for `position`.
        static auto for_position(Position position) noexcept -> Range;

        // Dummy range for mock purposes.
        static auto dummy() noexcept -> Range;

        [[nodiscard]] auto operator==(Range const&) const -> bool = default;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#location
    struct Location {
        Document_id doc_id;
        Range       range;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentPositionParams
    struct Character_location {
        Document_id doc_id;
        Position    position;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
    enum struct Severity : std::uint8_t { Error, Warning, Hint, Information };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticRelatedInformation
    struct Diagnostic_related {
        std::string message;
        Location    location;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
    struct Diagnostic {
        std::string                     message;
        Range                           range;
        Severity                        severity {};
        std::vector<Diagnostic_related> related_info;
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
        std::uint32_t       length;
        Semantic_token_type type;
    };

    // If a document is owned by a client, the server will not attempt to read it from disk.
    enum struct Document_ownership : std::uint8_t { Server, Client };

    // Description of a project.
    struct Manifest {
        std::filesystem::path root_path;
        std::string           main_name      = "main";
        std::string           file_extension = "kieli";
    };

    // In-memory representation of a text document.
    struct Document {
        std::string                     text;
        std::vector<Diagnostic>         diagnostics;
        std::vector<Semantic_token>     semantic_tokens;
        std::filesystem::file_time_type time;
        Document_ownership              ownership {};
        std::size_t                     revision {};
    };

    using Documents      = utl::Index_vector<Document_id, Document>;
    using Document_paths = std::unordered_map<std::filesystem::path, Document_id>;

    // Compilation database.
    struct Database {
        Documents        documents;
        Document_paths   paths;
        utl::String_pool string_pool;
        Manifest         manifest;
    };

    // Represents a file read failure.
    enum struct Read_failure : std::uint8_t { Does_not_exist, Failed_to_open, Failed_to_read };

    // Create a database.
    [[nodiscard]] auto database(Manifest manifest) -> Database;

    // Create a new document.
    [[nodiscard]] auto document(std::string text, Document_ownership ownership) -> ki::Document;

    // Find the `path` corresponding to the document identified by `doc_id`.
    [[nodiscard]] auto document_path(Database const& db, Document_id doc_id)
        -> std::filesystem::path const&;

    // Map `path` to `document`.
    [[nodiscard]] auto set_document(Database& db, std::filesystem::path path, Document document)
        -> Document_id;

    // Map `path` to a client-owned document with `text`.
    [[nodiscard]] auto client_open_document(
        Database& db, std::filesystem::path path, std::string text) -> Document_id;

    // If the document identified by `doc_id` is open and owned by a client, deallocate it.
    void client_close_document(Database& db, Document_id doc_id);

    // Creates a temporary document with `text`.
    [[nodiscard]] auto test_document(Database& db, std::string text) -> Document_id;

    // Attempt to read the file at `path`.
    [[nodiscard]] auto read_file(std::filesystem::path const& path)
        -> std::expected<std::string, Read_failure>;

    // Attempt to create a new document with server ownership by reading the file at `path`.
    [[nodiscard]] auto read_document(Database& db, std::filesystem::path path)
        -> std::expected<Document_id, Read_failure>;

    // Describe a file read failure.
    [[nodiscard]] auto describe_read_failure(Read_failure failure) -> std::string_view;

    // Find the substring of `string` corresponding to `range`.
    [[nodiscard]] auto text_range(std::string_view text, Range range) -> std::string_view;

    // Replace `range` in `text` with `new_text`.
    void edit_text(std::string& text, Range range, std::string_view new_text);

    // Add `diagnostic` to the document identified by `doc_id`.
    void add_diagnostic(Database& db, Document_id doc_id, Diagnostic diagnostic);

    // Add an error diagnostic to the document identified by `doc_id`.
    void add_error(Database& db, Document_id doc_id, Range range, std::string message);

    // Construct an error diagnostic.
    [[nodiscard]] auto error(Range range, std::string message) -> Diagnostic;

    // Construct a warning diagnostic.
    [[nodiscard]] auto warning(Range range, std::string message) -> Diagnostic;

    // Check whether `position` is contained within `range`.
    [[nodiscard]] auto range_contains(Range range, Position position) noexcept -> bool;

    // Check whether `range` occupies more than one line of source code.
    [[nodiscard]] auto is_multiline(Range range) noexcept -> bool;

    // Print diagnostics belonging to the document identified by `doc_id` to `stream`.
    void print_diagnostics(std::FILE* stream, Database const& db, Document_id doc_id);

    // Identifier with a range.
    struct Name {
        utl::String_id id;
        Range          range;
    };

    struct Upper : Name {};

    struct Lower : Name {};

    struct Error {};

    struct Integer {
        std::int64_t value {};
    };

    struct Floating {
        double value {};
    };

    struct Boolean {
        bool value {};
    };

    struct String {
        utl::String_id id;
    };

    // Represents concrete mutability.
    enum struct Mutability : std::uint8_t { Mut, Immut };

    // Returns `mut` or `immut`.
    auto mutability_string(Mutability mutability) -> std::string_view;

    // Severity to string.
    auto severity_string(Severity severity) -> std::string_view;

    // Check whether `name` starts with a capital letter, possibly preceded by underscores.
    auto is_uppercase(std::string_view name) -> bool;

} // namespace ki

template <>
struct std::formatter<ki::Position> {
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(ki::Position const position, auto& ctx)
    {
        return std::format_to(ctx.out(), "{}:{}", position.line, position.column);
    }
};

template <>
struct std::formatter<ki::Range> {
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(ki::Range const& range, auto& ctx)
    {
        return std::format_to(ctx.out(), "{}-{}", range.start, range.stop);
    }
};

#endif // KIELI_LIBCOMPILER_COMPILER
