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
        hir::Type_variable_kind kind {};
        hir::Type_variable_id   var_id;
        hir::Type_id            type_id;
        lsp::Range              origin;
        bool                    is_solved {};
    };

    struct Mutability_variable_data {
        hir::Mutability_variable_id var_id;
        hir::Mutability_id          mut_id;
        lsp::Range                  origin;
        bool                        is_solved {};
    };

    using Type_variables = utl::Index_vector<hir::Type_variable_id, Type_variable_data>;
    using Mutability_variables
        = utl::Index_vector<hir::Mutability_variable_id, Mutability_variable_data>;

    struct Inference_state {
        Type_variables       type_vars;
        utl::Disjoint_set    type_var_set;
        Mutability_variables mut_vars;
        utl::Disjoint_set    mut_var_set;
    };

    struct Tags {
        std::uint32_t current_template_parameter_tag {};
    };

    using Signature_scope_map
        = std::unordered_map<hir::Function_id, db::Environment_id, utl::Hash_vector_index>;

    struct Context {
        db::Arena                       arena;
        Tags                            tags;
        Constants                       constants;
        Signature_scope_map             signature_scope_map;
        std::vector<db::Environment_id> recycled_env_ids;
        db::Environment_id              root_env_id;
        db::Document_id                 doc_id;
    };

    auto context(db::Document_id doc_id) -> Context;

    auto make_constants(hir::Arena& arena) -> Constants;

    auto fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag;

    auto fresh_general_type_variable(Inference_state& state, hir::Arena& arena, lsp::Range origin)
        -> hir::Type;

    auto fresh_integral_type_variable(Inference_state& state, hir::Arena& arena, lsp::Range origin)
        -> hir::Type;

    auto fresh_mutability_variable(Inference_state& state, hir::Arena& arena, lsp::Range origin)
        -> hir::Mutability;

    void flatten_type(Context& ctx, Inference_state& state, hir::Type_variant& type);

    void set_type_solution(
        db::Database&       db,
        Context&            ctx,
        Inference_state&    state,
        Type_variable_data& variable_data,
        hir::Type_variant   solution);

    void set_mut_solution(
        Context&                  ctx,
        Inference_state&          state,
        Mutability_variable_data& variable_data,
        hir::Mutability_variant   solution);

    void ensure_no_unsolved_variables(db::Database& db, Context& ctx, Inference_state& state);

    // Collect definitions from the active document.
    // Returns a vector of symbols in the order they should be resolved.
    auto collect_document(db::Database& db, Context& ctx) -> std::vector<db::Symbol_id>;

    auto resolve_enumeration(db::Database& db, Context& ctx, hir::Enumeration_id id)
        -> hir::Enumeration&;

    auto resolve_concept(db::Database& db, Context& ctx, hir::Concept_id id) -> hir::Concept&;

    auto resolve_alias(db::Database& db, Context& ctx, hir::Alias_id id) -> hir::Alias&;

    auto resolve_function_body(db::Database& db, Context& ctx, hir::Function_id id)
        -> hir::Expression&;

    auto resolve_function_signature(db::Database& db, Context& ctx, hir::Function_id id)
        -> hir::Function_signature&;

    auto resolve_template_parameters(
        db::Database&                   db,
        Context&                        ctx,
        Inference_state&                state,
        db::Environment_id              env_id,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        db::Database&                               db,
        Context&                                    ctx,
        Inference_state&                            state,
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
        Inference_state&   state,
        db::Environment_id env_id,
        ast::Path const&   path) -> db::Symbol_id;

    auto resolve_expression(
        db::Database&          db,
        Context&               ctx,
        Inference_state&       state,
        db::Environment_id     env_id,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        db::Database&       db,
        Context&            ctx,
        Inference_state&    state,
        db::Environment_id  env_id,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        db::Database&      db,
        Context&           ctx,
        Inference_state&   state,
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
        Inference_state&         state,
        lsp::Range               range,
        hir::Type_variant const& sub,
        hir::Type_variant const& super);

    // Get the HIR representation of the error type with `range`.
    auto error_type(Constants const& constants, lsp::Range range) -> hir::Type;

    // Get the HIR representation of an error expression with `range`.
    auto error_expression(Constants const& constants, lsp::Range range) -> hir::Expression;

    // Get the HIR representation of a unit tuple expression with `range`.
    auto unit_expression(Constants const& constants, lsp::Range range) -> hir::Expression;

    // Get a new or recycled id for the given environment.
    auto new_environment(Context& ctx, db::Environment env) -> db::Environment_id;

    // Create a new local environment.
    auto new_scope(Context& ctx, db::Environment_id parent_id) -> db::Environment_id;

    // If the given symbol is unused, emit a warning.
    void warn_if_unused(db::Database& db, Context& ctx, db::Symbol_id symbol_id);

    // Emit warnings for any unused bindings.
    void report_unused(db::Database& db, Context& ctx, db::Environment_id env_id);

    // Mark the given environment as reusable.
    void recycle_environment(Context& ctx, db::Environment_id env_id);

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
