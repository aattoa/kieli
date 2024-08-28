#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <cpputil/mem/stable_string_pool.hpp>
#include <cppdiag/cppdiag.hpp> // TODO: remove

namespace kieli {

    // Identifies a document.
    struct Document_id : utl::Vector_index<Document_id> {
        using Vector_index::Vector_index;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
    struct Position {
        std::uint32_t line {};
        std::uint32_t column {};

        // Advance this position with `character`.
        auto advance_with(char character) noexcept -> void;

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
        Document_id document_id;
        Range       range;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentPositionParams
    struct Character_location {
        Document_id document_id;
        Position    position;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
    enum class Severity { error, warning, hint, information };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticRelatedInformation
    struct Diagnostic_related_info {
        std::string message;
        Location    location;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
    struct Diagnostic {
        std::string                          message;
        Range                                range;
        Severity                             severity {};
        std::vector<Diagnostic_related_info> related_info;
    };

    // If a document is owned by a client, the server will not attempt to read it from disk.
    enum class Document_ownership { server, client };

    // In-memory representation of a text document.
    struct Document {
        std::string                     text;
        std::vector<Diagnostic>         diagnostics;
        std::filesystem::file_time_type time;
        Document_ownership              ownership {};
        std::size_t                     revision {};
    };

    using Document_map   = std::unordered_map<Document_id, Document, utl::Hash_vector_index>;
    using Document_paths = utl::Index_vector<Document_id, std::filesystem::path>;
    using String_pool    = cpputil::mem::Stable_string_pool;

    // Compilation database.
    struct Database {
        Document_map   documents;
        Document_paths paths;
        String_pool    string_pool;
    };

    // Represents a file read failure.
    enum class Read_failure { does_not_exist, failed_to_open, failed_to_read };

    // Retrieve the `Document_id` corresponding to `path`.
    [[nodiscard]] auto document_id(Database& db, std::filesystem::path path) -> Document_id;

    // If `db` contains a document with `path`, return its `Document_id`.
    [[nodiscard]] auto find_existing_document_id(
        Database const& db, std::filesystem::path const& path) -> std::optional<Document_id>;

    // Map `path` to `document`.
    [[nodiscard]] auto set_document(Database& db, std::filesystem::path path, Document document)
        -> Document_id;

    // Map `path` to a client-owned document with `text`.
    [[nodiscard]] auto client_open_document(
        Database& db, std::filesystem::path path, std::string text) -> Document_id;

    // If the document identified by `document_id` is open and owned by a client, deallocate it.
    auto client_close_document(Database& db, Document_id document_id) -> void;

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
    auto edit_text(std::string& text, Range range, std::string_view new_text) -> void;

    // Add `diagnostic` to the document identified by `document_id`.
    auto add_diagnostic(Database& db, Document_id document_id, Diagnostic diagnostic) -> void;

    // Add an error diagnostic to the document identified by `document_id`.
    auto add_error(Database& db, Document_id document_id, Range range, std::string message) -> void;

    // Construct an error diagnostic.
    [[nodiscard]] auto error(Range range, std::string message) -> Diagnostic;

    // Format the diagnostics belonging to the document identified by `document_id`.
    [[nodiscard]] auto format_document_diagnostics(
        Database const& db,
        Document_id     document_id,
        cppdiag::Colors colors = cppdiag::Colors::none()) -> std::string;

    // Pooled string that is cheap to copy and compare.
    using Identifier = cpputil::mem::Stable_pool_string;

    // Identifier with a range.
    struct Name {
        Identifier identifier;
        Range      range;

        // Check whether this name starts with a capital letter, possibly preceded by underscores.
        [[nodiscard]] auto is_upper() const -> bool;

        // Compare identifiers.
        [[nodiscard]] auto operator==(Name const&) const noexcept -> bool;
    };

    struct Upper : Name {};

    struct Lower : Name {};

    struct Integer {
        std::uint64_t value {};
    };

    struct Floating {
        double value {};
    };

    struct Boolean {
        bool value {};
    };

    struct Character {
        char value {};
    };

    struct String {
        cpputil::mem::Stable_pool_string value;
    };

    template <class T>
    concept literal = utl::one_of<T, Integer, Floating, Boolean, Character, String>;

    enum class Mutability { mut, immut };

} // namespace kieli

template <utl::one_of<kieli::Name, kieli::Upper, kieli::Lower> Name>
struct std::formatter<Name> : std::formatter<std::string_view> {
    auto format(Name const& name, auto& context) const
    {
        return std::formatter<std::string_view>::format(name.identifier.view(), context);
    }
};

template <utl::one_of<kieli::Integer, kieli::Floating, kieli::Boolean> Literal>
struct std::formatter<Literal> : std::formatter<decltype(Literal::value)> {
    auto format(Literal const literal, auto& context) const
    {
        return std::formatter<decltype(Literal::value)>::format(literal.value, context);
    }
};

template <utl::one_of<kieli::Character, kieli::String> Literal>
struct std::formatter<Literal> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(Literal const& literal, auto& context)
    {
        if constexpr (std::is_same_v<Literal, kieli::Character>) {
            return std::format_to(context.out(), "'{}'", literal.value);
        }
        else if constexpr (std::is_same_v<Literal, kieli::String>) {
            return std::format_to(context.out(), "\"{}\"", literal.value);
        }
        else {
            static_assert(false);
        }
    }
};

template <>
struct std::formatter<kieli::Mutability> : std::formatter<std::string_view> {
    auto format(kieli::Mutability const mut, auto& context) const
    {
        std::string_view const string = mut == kieli::Mutability::mut ? "mut" : "immut";
        return std::formatter<std::string_view>::format(string, context);
    }
};

template <>
struct std::formatter<kieli::Position> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(kieli::Position const position, auto& context)
    {
        return std::format_to(context.out(), "{}:{}", position.line, position.column);
    }
};

template <>
struct std::formatter<kieli::Range> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(kieli::Range const& range, auto& context)
    {
        return std::format_to(context.out(), "({}-{})", range.start, range.stop);
    }
};
