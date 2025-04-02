#ifndef KIELI_LIBRESOLVE_RESOLVE
#define KIELI_LIBRESOLVE_RESOLVE

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/disjoint_set.hpp>
#include <libresolve/module.hpp>

namespace ki::resolve {

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
        Range                   origin;
        bool                    is_solved {};
    };

    struct Mutability_variable_data {
        hir::Mutability_variable_id var_id;
        hir::Mutability_id          mut_id;
        Range                       origin;
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
        Document_id          doc_id;
    };

    struct Tags {
        std::size_t current_template_parameter_tag {};
        std::size_t current_local_variable_tag {};
    };

    struct Document_info {
        cst::Arena cst;
        ast::Arena ast;
    };

    using Document_info_map
        = std::unordered_map<Document_id, Document_info, utl::Hash_vector_index>;

    struct Context {
        Database&         db;
        hir::Arena        hir;
        Info_arena        info;
        Tags              tags;
        Constants         constants;
        Document_info_map documents;
    };

    auto context(Database& db) -> Context;

    auto make_constants(hir::Arena& arena) -> Constants;

    auto make_inference_state(Document_id doc_id) -> Inference_state;

    auto fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag;

    auto fresh_local_variable_tag(Tags& tags) -> hir::Local_variable_tag;

    auto fresh_general_type_variable(Inference_state& state, hir::Arena& arena, Range origin)
        -> hir::Type;

    auto fresh_integral_type_variable(Inference_state& state, hir::Arena& arena, Range origin)
        -> hir::Type;

    auto fresh_mutability_variable(Inference_state& state, hir::Arena& arena, Range origin)
        -> hir::Mutability;

    void flatten_type(Context& ctx, Inference_state& state, hir::Type_variant& type);

    void set_solution(
        Context&            ctx,
        Inference_state&    state,
        Type_variable_data& variable_data,
        hir::Type_variant   solution);

    void set_solution(
        Context&                  ctx,
        Inference_state&          state,
        Mutability_variable_data& variable_data,
        hir::Mutability_variant   solution);

    void ensure_no_unsolved_variables(Context& ctx, Inference_state& state);

    auto collect_document(Context& ctx, Document_id doc_id) -> hir::Environment_id;

    void resolve_environment(Context& ctx, hir::Environment_id env_id);

    auto resolve_enumeration(Context& ctx, Enumeration_info& info) -> hir::Enumeration&;

    auto resolve_concept(Context& ctx, Concept_info& info) -> hir::Concept&;

    auto resolve_alias(Context& ctx, Alias_info& info) -> hir::Alias&;

    auto resolve_function_body(Context& ctx, Function_info& info) -> hir::Expression&;

    auto resolve_function_signature(Context& ctx, Function_info& info) -> hir::Function_signature&;

    // Resolve template parameters and register them in the given scope.
    auto resolve_template_parameters(
        Context&                        ctx,
        Inference_state&                state,
        hir::Scope_id                   scope_id,
        hir::Environment_id             env_id,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        Context&                                    ctx,
        Inference_state&                            state,
        hir::Scope_id                               scope_id,
        hir::Environment_id                         env_id,
        std::vector<hir::Template_parameter> const& parameters,
        std::vector<ast::Template_argument> const&  arguments)
        -> std::vector<hir::Template_argument>;

    auto resolve_mutability(Context& ctx, hir::Scope_id scope_id, ast::Mutability const& mutability)
        -> hir::Mutability;

    auto resolve_expression(
        Context&               ctx,
        Inference_state&       state,
        hir::Scope_id          scope_id,
        hir::Environment_id    env_id,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        Context&            ctx,
        Inference_state&    state,
        hir::Scope_id       scope_id,
        hir::Environment_id env_id,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        Context&            ctx,
        Inference_state&    state,
        hir::Scope_id       scope_id,
        hir::Environment_id env_id,
        ast::Type const&    type) -> hir::Type;

    auto resolve_concept_reference(
        Context&            ctx,
        Inference_state&    state,
        hir::Scope_id       scope_id,
        hir::Environment_id env_id,
        ast::Path const&    path) -> hir::Concept_id;

    void bind_mutability(Scope& scope, Mutability_bind bind);
    void bind_variable(Scope& scope, Variable_bind bind);
    void bind_type(Scope& scope, Type_bind bind);

    // Emit warnings for any unused bindings.
    void report_unused(Database& db, Scope& scope);

    auto find_mutability(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id)
        -> Mutability_bind*;
    auto find_variable(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id)
        -> Variable_bind*;
    auto find_type(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id) -> Type_bind*;

    // Check whether a type variable with `tag` occurs in `type`.
    auto occurs_check(
        hir::Arena const& arena, hir::Type_variable_id id, hir::Type_variant const& type) -> bool;

    // Require that `sub` is equal to or a subtype of `super`.
    void require_subtype_relationship(
        Context&                 ctx,
        Inference_state&         state,
        Range                    range,
        hir::Type_variant const& sub,
        hir::Type_variant const& super);

    // Get the HIR representation of the error type with `range`.
    auto error_type(Constants const& constants, Range range) -> hir::Type;

    // Get the HIR representation of an error expression with `range`.
    auto error_expression(Constants const& constants, Range range) -> hir::Expression;

    // Get the HIR representation of a unit tuple expression with `range`.
    auto unit_expression(Constants const& constants, Range range) -> hir::Expression;

    // Invoke `callback` with a temporary `Scope_id`.
    auto scope(Context& ctx, Scope scope, std::invocable<hir::Scope_id> auto const& callback)
    {
        auto scope_id = ctx.info.scopes.push(std::move(scope));
        auto result   = std::invoke(callback, scope_id);
        ctx.info.scopes.kill(scope_id);
        return result;
    }

    template <std::invocable<hir::Scope_id> Callback>
    auto child_scope(Context& ctx, hir::Scope_id const parent_id, Callback const& callback)
    {
        Scope child {
            .variables    = {},
            .types        = {},
            .mutabilities = {},
            .doc_id       = ctx.info.scopes.index_vector[parent_id].doc_id,
            .parent_id    = parent_id,
        };
        return scope(ctx, std::move(child), callback);
    }

    void debug_display_environment(Context const& ctx, hir::Environment_id env_id);

} // namespace ki::resolve

#endif // KIELI_LIBRESOLVE_RESOLVE
