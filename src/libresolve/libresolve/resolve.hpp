#ifndef KIELI_LIBRESOLVE_RESOLVE
#define KIELI_LIBRESOLVE_RESOLVE

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/disjoint_set.hpp>
#include <libcompiler/db.hpp>

namespace ki::res {

    struct Scope_id : utl::Vector_index<Scope_id, std::uint32_t> {
        using Vector_index::Vector_index;
    };

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

    struct Scope {
        hir::Identifier_map<hir::Local_variable_id>   variables;
        hir::Identifier_map<hir::Local_type_id>       types;
        hir::Identifier_map<hir::Local_mutability_id> mutabilities;
        db::Document_id                               doc_id;
        std::optional<Scope_id>                       parent_id;
    };

    using Scope_map   = std::unordered_map<hir::Function_id, Scope_id, utl::Hash_vector_index>;
    using Scope_arena = utl::Index_arena<Scope_id, Scope>;

    struct Context {
        hir::Arena      hir;
        Tags            tags;
        Constants       constants;
        Scope_arena     scopes;
        Scope_map       scope_map;
        db::Document_id doc_id;
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

    auto collect_document(db::Database& db, Context& ctx, db::Document_id doc_id)
        -> hir::Environment_id;

    void resolve_environment(db::Database& db, Context& ctx, hir::Environment_id env_id);

    auto resolve_enumeration(db::Database& db, Context& ctx, hir::Enumeration_id id)
        -> hir::Enumeration&;

    auto resolve_concept(db::Database& db, Context& ctx, hir::Concept_id id) -> hir::Concept&;

    auto resolve_alias(db::Database& db, Context& ctx, hir::Alias_id id) -> hir::Alias&;

    auto resolve_function_body(db::Database& db, Context& ctx, hir::Function_id id)
        -> hir::Expression&;

    auto resolve_function_signature(db::Database& db, Context& ctx, hir::Function_id id)
        -> hir::Function_signature&;

    // Resolve template parameters and register them in the given scope.
    auto resolve_template_parameters(
        db::Database&                   db,
        Context&                        ctx,
        Inference_state&                state,
        Scope_id                        scope_id,
        hir::Environment_id             env_id,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        db::Database&                               db,
        Context&                                    ctx,
        Inference_state&                            state,
        Scope_id                                    scope_id,
        hir::Environment_id                         env_id,
        std::vector<hir::Template_parameter> const& parameters,
        std::vector<ast::Template_argument> const&  arguments)
        -> std::vector<hir::Template_argument>;

    auto resolve_mutability(
        db::Database& db, Context& ctx, Scope_id scope_id, ast::Mutability const& mutability)
        -> hir::Mutability;

    auto resolve_path(
        db::Database&       db,
        Context&            ctx,
        Inference_state&    state,
        Scope_id            scope_id,
        hir::Environment_id env_id,
        ast::Path const&    path) -> hir::Symbol;

    auto resolve_expression(
        db::Database&          db,
        Context&               ctx,
        Inference_state&       state,
        Scope_id               scope_id,
        hir::Environment_id    env_id,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        db::Database&       db,
        Context&            ctx,
        Inference_state&    state,
        Scope_id            scope_id,
        hir::Environment_id env_id,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        db::Database&       db,
        Context&            ctx,
        Inference_state&    state,
        Scope_id            scope_id,
        hir::Environment_id env_id,
        ast::Type const&    type) -> hir::Type;

    auto bind_local_variable(
        db::Database&       db,
        Context&            ctx,
        Scope_id            scope_id,
        db::Lower           name,
        hir::Local_variable variable) -> hir::Local_variable_id;

    auto bind_local_mutability(
        db::Database&         db,
        Context&              ctx,
        Scope_id              scope_id,
        db::Lower             name,
        hir::Local_mutability mutability) -> hir::Local_mutability_id;

    auto bind_local_type(
        db::Database& db, Context& ctx, Scope_id scope_id, db::Upper name, hir::Local_type type)
        -> hir::Local_type_id;

    auto find_local_mutability(Context& ctx, Scope_id scope_id, utl::String_id name_id)
        -> std::optional<hir::Local_mutability_id>;

    auto find_local_variable(Context& ctx, Scope_id scope_id, utl::String_id name_id)
        -> std::optional<hir::Local_variable_id>;

    auto find_local_type(Context& ctx, Scope_id scope_id, utl::String_id name_id)
        -> std::optional<hir::Local_type_id>;

    // Emit warnings for any unused bindings.
    void report_unused(db::Database& db, Context& ctx, Scope_id scope_id);

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

    // Create an empty scope with no parent.
    auto make_scope(db::Document_id doc_id) -> Scope;

    // Invoke `callback` with a temporary `Scope_id`.
    auto scope(Context& ctx, Scope scope, std::invocable<Scope_id> auto const& callback)
    {
        auto scope_id = ctx.scopes.push(std::move(scope));
        auto result   = std::invoke(callback, scope_id);
        ctx.scopes.kill(scope_id);
        return result;
    }

    template <std::invocable<Scope_id> Callback>
    auto child_scope(Context& ctx, Scope_id const parent_id, Callback const& callback)
    {
        Scope child {
            .variables    = {},
            .types        = {},
            .mutabilities = {},
            .doc_id       = ctx.scopes.index_vector[parent_id].doc_id,
            .parent_id    = parent_id,
        };
        return scope(ctx, std::move(child), callback);
    }

    void debug_display_environment(
        db::Database const& db, Context const& ctx, hir::Environment_id env_id);

} // namespace ki::res

#endif // KIELI_LIBRESOLVE_RESOLVE
