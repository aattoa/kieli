#ifndef KIELI_LIBCOMPILER_DB
#define KIELI_LIBCOMPILER_DB

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/string_pool.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/hir/hir.hpp>
#include <libcompiler/fwd.hpp>
#include <libcompiler/lsp.hpp>

namespace ki::db {

    struct Symbol_variant
        : std::variant<
              Error,
              hir::Function_id,
              hir::Structure_id,
              hir::Enumeration_id,
              hir::Constructor_id,
              hir::Field_id,
              hir::Concept_id,
              hir::Alias_id,
              hir::Module_id,
              hir::Local_variable_id,
              hir::Local_mutability_id,
              hir::Local_type_id> {
        using variant::variant;
        auto operator==(Symbol_variant const&) const -> bool = default;
    };

    struct Symbol {
        Symbol_variant variant;
        Name           name;
        std::uint32_t  use_count {};
    };

    using Environment_map = std::unordered_map<utl::String_id, Symbol_id, utl::Hash_vector_index>;

    enum struct Environment_kind : std::uint8_t { Root, Module, Scope, Type };

    struct Environment {
        Environment_map               map;
        std::optional<Environment_id> parent_id;
        std::optional<utl::String_id> name_id;
        Document_id                   doc_id;
        Environment_kind              kind {};
    };

    // If a document is owned by a client, the server will not attempt to read it from disk.
    enum struct Ownership : std::uint8_t { Server, Client };

    // Inlay type or parameter hint.
    struct Inlay_hint {
        lsp::Position                               position;
        std::variant<hir::Type_id, hir::Pattern_id> variant;
    };

    // Insert an underscore to silence an unused symbol warning.
    struct Action_silence_unused {
        Symbol_id symbol_id;
    };

    // Insert missing struct fields in a struct initializer.
    struct Action_fill_in_struct_init {
        std::vector<hir::Field_id>   field_ids;
        std::optional<lsp::Position> final_field_end;
    };

    struct Action_variant : std::variant<Action_silence_unused, Action_fill_in_struct_init> {
        using variant::variant;
    };

    // A code action.
    struct Action {
        Action_variant variant;
        lsp::Range     range;
    };

    // A reference to a symbol. Used to determine the symbol at a particular position.
    struct Symbol_reference {
        lsp::Reference reference;
        Symbol_id      symbol_id;
    };

    // Arenas necessary for semantic analysis.
    struct Arena {
        cst::Arena cst;
        ast::Arena ast;
        hir::Arena hir;

        utl::Index_vector<Environment_id, Environment> environments;
        utl::Index_vector<Symbol_id, Symbol>           symbols;
    };

    // Information collected during analysis.
    struct Document_info {
        std::vector<lsp::Diagnostic>     diagnostics;
        std::vector<lsp::Semantic_token> semantic_tokens;
        std::vector<Inlay_hint>          inlay_hints;
        std::vector<Symbol_reference>    references;
        std::vector<Action>              actions;
        std::optional<Environment_id>    root_env_id;
    };

    // In-memory representation of a text document.
    struct Document {
        Document_info info;
        std::string   text;
        Arena         arena;
        Ownership     ownership {};
        std::size_t   revision {};
    };

    // Description of a project.
    struct Manifest {
        std::filesystem::path root_path;
        std::string           main_name = "main";
        std::string           extension = "ki";
    };

    // Compiler database.
    struct Database {
        utl::Index_vector<Document_id, Document>               documents;
        std::unordered_map<std::filesystem::path, Document_id> paths;
        utl::String_pool                                       string_pool;
        Manifest                                               manifest;
    };

    // Represents a file read failure.
    enum struct Read_failure : std::uint8_t { Does_not_exist, Failed_to_open, Failed_to_read };

    // Create a compiler database.
    [[nodiscard]] auto database(Manifest manifest) -> Database;

    // Create a new document.
    [[nodiscard]] auto document(std::string text, Ownership ownership) -> Document;

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

    // Describe the symbol kind.
    [[nodiscard]] auto describe_symbol_kind(Symbol_variant variant) -> std::string_view;

    // Find the substring of `string` corresponding to `range`.
    [[nodiscard]] auto text_range(std::string_view text, lsp::Range range) -> std::string_view;

    // Replace `range` in `text` with `new_text`.
    void edit_text(std::string& text, lsp::Range range, std::string_view new_text);

    // Add a type hint to the document identified by `doc_id`.
    void add_type_hint(Database& db, Document_id doc_id, lsp::Position pos, hir::Type_id type_id);

    // Add a parameter hint to the document identified by `doc_id`.
    void add_param_hint(Database& db, Document_id doc_id, lsp::Position pos, hir::Pattern_id param);

    // Add a code action to the document identified by `doc_id`.
    void add_action(Database& db, Document_id doc_id, lsp::Range range, Action_variant variant);

    // Add a symbol reference to the document identified by `doc_id`.
    void add_reference(Database& db, Document_id doc_id, lsp::Reference ref, Symbol_id symbol_id);

    // Add `diagnostic` to the document identified by `doc_id`.
    void add_diagnostic(Database& db, Document_id doc_id, lsp::Diagnostic diagnostic);

    // Add an error diagnostic to the document identified by `doc_id`.
    void add_error(Database& db, Document_id doc_id, lsp::Range range, std::string message);

    // Print diagnostics belonging to the document identified by `doc_id` to `stream`.
    void print_diagnostics(std::ostream& stream, Database const& db, Document_id doc_id);

} // namespace ki::db

#endif // KIELI_LIBCOMPILER_DB
