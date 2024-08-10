#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <cpputil/mem/stable_string_pool.hpp>
#include <cppdiag/cppdiag.hpp> // TODO: remove

namespace kieli {

    // Thrown when an unrecoverable error is encountered. To be removed.
    struct Compilation_failure : std::exception {
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    // Identifies a `Document`.
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
        std::string             text;
        std::vector<Diagnostic> diagnostics;
        std::size_t             revision {};
        Document_ownership      ownership {};
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

    // Access the document stored in `db` identified by `id`.
    auto document(Database& db, Document_id id) -> Document&;

    // Add a new `Document` with the given `path` and `text` to `db`.
    // Precondition: `db` must not contain a `Document` with `path`.
    [[nodiscard]] auto add_document(
        Database&             db,
        std::filesystem::path path,
        std::string           text,
        Document_ownership    ownership = Document_ownership::server) -> Document_id;

    // If `db` contains a `Document` with `path`, return its `Document_id`.
    [[nodiscard]] auto find_document(Database const& db, std::filesystem::path const& path)
        -> std::optional<Document_id>;

    // Attempt to read the file at `path`.
    [[nodiscard]] auto read_file(std::filesystem::path const& path)
        -> std::expected<std::string, Read_failure>;

    // Attempt to create a new `Document` by reading the file at `path`.
    // Precondition: `db` must not contain a `Document` with `path`.
    [[nodiscard]] auto read_document(Database& db, std::filesystem::path path)
        -> std::expected<Document_id, Read_failure>;

    // Describe a file read failure.
    [[nodiscard]] auto describe_read_failure(Read_failure failure) -> std::string_view;

    // Find the substring of `string` corresponding to `range`.
    [[nodiscard]] auto text_range(std::string_view string, Range range) -> std::string_view;

    // Replace `range` in `text` with `new_text`.
    auto edit_text(std::string& text, Range range, std::string_view new_text) -> void;

    // Emit `diagnostic` and terminate compilation by throwing `Compilation_failure`. To be removed.
    [[noreturn]] auto fatal_error(Database& db, Document_id document_id, Diagnostic diagnostic)
        -> void;

    // Emit a diagnostic and terminate compilation by throwing `Compilation_failure`. To be removed.
    [[noreturn]] auto fatal_error(
        Database& db, Document_id document_id, Range range, std::string message) -> void;

    [[nodiscard]] auto format_diagnostics(
        std::filesystem::path const& path,
        Document const&              document,
        cppdiag::Colors              colors = cppdiag::Colors::none()) -> std::string;

    using Identifier = cpputil::mem::Stable_pool_string;

    struct Name {
        Identifier identifier;
        Range      range;

        // Check whether this name starts with an upper-case letter,
        // possibly preceded by underscores.
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

    namespace type {
        enum class Integer { i8, i16, i32, i64, u8, u16, u32, u64, _enumerator_count };

        struct Floating {};

        struct Boolean {};

        struct Character {};

        struct String {};

        auto integer_name(Integer) noexcept -> std::string_view;
    } // namespace type

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
