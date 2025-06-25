#ifndef KIELI_LIBRESOLVE_RESOLVE
#define KIELI_LIBRESOLVE_RESOLVE

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/disjoint_set.hpp>
#include <libcompiler/db.hpp>

namespace ki::res {

    struct Constants {
        hir::Type_id       type_i8;
        hir::Type_id       type_i16;
        hir::Type_id       type_i32;
        hir::Type_id       type_i64;
        hir::Type_id       type_u8;
        hir::Type_id       type_u16;
        hir::Type_id       type_u32;
        hir::Type_id       type_u64;
        hir::Type_id       type_boolean;
        hir::Type_id       type_floating;
        hir::Type_id       type_string;
        hir::Type_id       type_character;
        hir::Type_id       type_unit;
        hir::Type_id       type_error;
        hir::Mutability_id mut_yes;
        hir::Mutability_id mut_no;
        hir::Mutability_id mut_error;
    };

    struct Type_variable_data {
        hir::Type_variable_id   var_id;
        hir::Type_id            type_id;
        lsp::Range              origin;
        hir::Type_variable_kind kind {};
        bool                    is_solved {};
    };

    struct Mutability_variable_data {
        hir::Mutability_variable_id var_id;
        hir::Mutability_id          mut_id;
        lsp::Range                  origin;
        bool                        is_solved {};
    };

    struct Loop_info {
        std::optional<hir::Type_id> result_type_id;
        ast::Loop_source            source {};
    };

    struct Block_state {
        std::vector<Type_variable_data>       type_vars;
        utl::Disjoint_set                     type_var_set;
        std::vector<Mutability_variable_data> mut_vars;
        utl::Disjoint_set                     mut_var_set;
        std::optional<Loop_info>              loop_info;
    };

    struct Tags {
        std::uint32_t current_template_parameter_tag {};
    };

    // Maps functions to their corresponding signature scopes.
    using Signature_scope_map
        = std::unordered_map<hir::Function_id, db::Environment_id, utl::Hash_vector_index>;

    // Resolution context for a single document.
    struct Context {
        db::Arena           arena;
        Constants           constants;
        Signature_scope_map signature_scope_map;
        db::Environment_id  root_env_id;
        db::Document_id     doc_id;
        Tags                tags;
    };

    // Create a resolution context for the given document.
    auto context(db::Document_id doc_id) -> Context;

    auto make_constants(hir::Arena& arena) -> Constants;

    auto fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag;

    // Create a fresh unification type variable for general use.
    auto fresh_general_type_variable(Context& ctx, Block_state& state, lsp::Range origin)
        -> hir::Type;

    // Create a fresh unification type variable for an integer literal.
    auto fresh_integral_type_variable(Context& ctx, Block_state& state, lsp::Range origin)
        -> hir::Type;

    // Create a fresh unification mutability variable.
    auto fresh_mutability_variable(Context& ctx, Block_state& state, lsp::Range origin)
        -> hir::Mutability;

    void flatten_type(Context& ctx, Block_state& state, hir::Type_variant& type);

    void set_type_solution(
        db::Database&         db,
        Context&              ctx,
        Block_state&          state,
        hir::Type_variable_id var_id,
        hir::Type_variant     solution);

    void set_mut_solution(
        db::Database&               db,
        Context&                    ctx,
        Block_state&                state,
        hir::Mutability_variable_id var_id,
        hir::Mutability_variant     solution);

    void ensure_no_unsolved_variables(db::Database& db, Context& ctx, Block_state& state);

    // Collect definitions from the active document.
    // Returns a vector of symbols in the order they should be resolved.
    auto collect_document(db::Database& db, Context& ctx) -> std::vector<db::Symbol_id>;

    auto resolve_structure(db::Database& db, Context& ctx, hir::Structure_id id) -> hir::Structure&;

    auto resolve_enumeration(db::Database& db, Context& ctx, hir::Enumeration_id id)
        -> hir::Enumeration&;

    auto resolve_concept(db::Database& db, Context& ctx, hir::Concept_id id) -> hir::Concept&;

    auto resolve_alias(db::Database& db, Context& ctx, hir::Alias_id id) -> hir::Alias&;

    auto resolve_function_body(db::Database& db, Context& ctx, hir::Function_id id)
        -> hir::Expression_id;

    auto resolve_function_signature(db::Database& db, Context& ctx, hir::Function_id id)
        -> hir::Function_signature&;

    auto resolve_template_parameters(
        db::Database&                   db,
        Context&                        ctx,
        Block_state&                    state,
        db::Environment_id              env_id,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        db::Database&                               db,
        Context&                                    ctx,
        Block_state&                                state,
        db::Environment_id                          env_id,
        std::vector<hir::Template_parameter> const& parameters,
        std::vector<ast::Template_argument> const&  arguments)
        -> std::vector<hir::Template_argument>;

    auto resolve_mutability(
        db::Database&          db,
        Context&               ctx,
        db::Environment_id     env_id,
        ast::Mutability const& mutability) -> hir::Mutability;

    auto resolve_path(
        db::Database&      db,
        Context&           ctx,
        Block_state&       state,
        db::Environment_id env_id,
        ast::Path const&   path) -> db::Symbol_id;

    auto resolve_expression(
        db::Database&          db,
        Context&               ctx,
        Block_state&           state,
        db::Environment_id     env_id,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        db::Database&       db,
        Context&            ctx,
        Block_state&        state,
        db::Environment_id  env_id,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        db::Database&      db,
        Context&           ctx,
        Block_state&       state,
        db::Environment_id env_id,
        ast::Type const&   type) -> hir::Type;

    void resolve_symbol(db::Database& db, Context& ctx, db::Symbol_id symbol_id);

    // Check whether a type variable with `tag` occurs in `type`.
    auto occurs_check(
        hir::Arena const& arena, hir::Type_variable_id id, hir::Type_variant const& type) -> bool;

    // Require that `sub` is equal to or a subtype of `super`.
    void require_subtype_relationship(
        db::Database&            db,
        Context&                 ctx,
        Block_state&             state,
        lsp::Range               range,
        hir::Type_variant const& sub,
        hir::Type_variant const& super);

    // Require that `sub` is equal to or a submutability of `super`.
    void require_submutability_relationship(
        db::Database&                  db,
        Context&                       ctx,
        Block_state&                   state,
        lsp::Range                     range,
        hir::Mutability_variant const& sub,
        hir::Mutability_variant const& super);

    // Get the HIR representation of the error type with `range`.
    auto error_type(Context const& ctx, lsp::Range range) -> hir::Type;

    // Get the HIR representation of an error expression with `range`.
    auto error_expression(Context const& ctx, lsp::Range range) -> hir::Expression;

    // Get the HIR representation of a unit tuple expression with `range`.
    auto unit_expression(Context const& ctx, lsp::Range range) -> hir::Expression;

    // Create a new local environment.
    auto new_scope(Context& ctx, db::Environment_id parent_id) -> db::Environment_id;

    // Create a new symbol.
    auto new_symbol(Context& ctx, db::Name name, db::Symbol_variant variant) -> db::Symbol_id;

    // If the given symbol is unused, emit a warning.
    void warn_if_unused(db::Database& db, Context& ctx, db::Symbol_id symbol_id);

    // Emit warnings for any unused bindings.
    void report_unused(db::Database& db, Context& ctx, db::Environment_id env_id);

    // Check whether the given symbol can be shadowed in the same environment.
    auto can_shadow(db::Symbol_variant variant) -> bool;

    // Bind a symbol to the given environment.
    auto bind_symbol(
        db::Database&      db,
        Context&           ctx,
        db::Environment_id env_id,
        db::Name           name,
        db::Symbol_variant variant) -> db::Symbol_id;

} // namespace ki::res

#endif // KIELI_LIBRESOLVE_RESOLVE
