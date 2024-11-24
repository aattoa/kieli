#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/disjoint_set.hpp>
#include <libresolve/module.hpp>

namespace libresolve {

    struct Constants {
        hir::Type_id       i8_type;
        hir::Type_id       i16_type;
        hir::Type_id       i32_type;
        hir::Type_id       i64_type;
        hir::Type_id       u8_type;
        hir::Type_id       u16_type;
        hir::Type_id       u32_type;
        hir::Type_id       u64_type;
        hir::Type_id       boolean_type;
        hir::Type_id       floating_type;
        hir::Type_id       string_type;
        hir::Type_id       character_type;
        hir::Type_id       unit_type;
        hir::Type_id       error_type;
        hir::Mutability_id mutability_yes;
        hir::Mutability_id mutability_no;
        hir::Mutability_id mutability_error;
    };

    struct Type_variable_data {
        hir::Type_variable_kind kind {};
        hir::Type_variable_id   variable_id;
        hir::Type_id            type_id;
        kieli::Range            origin;
        bool                    is_solved {};
    };

    struct Mutability_variable_data {
        hir::Mutability_variable_id variable_id;
        hir::Mutability_id          mutability_id;
        kieli::Range                origin;
        bool                        is_solved {};
    };

    using Type_variables = utl::Index_vector<hir::Type_variable_id, Type_variable_data>;
    using Mutability_variables
        = utl::Index_vector<hir::Mutability_variable_id, Mutability_variable_data>;

    struct Inference_state {
        Type_variables       type_variables;
        Mutability_variables mutability_variables;
        utl::Disjoint_set    type_variable_disjoint_set;
        utl::Disjoint_set    mutability_variable_disjoint_set;
        kieli::Document_id   document_id;
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
        = std::unordered_map<kieli::Document_id, Document_info, utl::Hash_vector_index>;

    struct Context {
        kieli::Database&  db;
        hir::Arena        hir;
        Info_arena        info;
        Tags              tags;
        Constants         constants;
        Document_info_map documents;
    };

    auto make_constants(hir::Arena& arena) -> Constants;

    auto fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag;

    auto fresh_local_variable_tag(Tags& tags) -> hir::Local_variable_tag;

    auto fresh_general_type_variable(Inference_state& state, hir::Arena& arena, kieli::Range origin)
        -> hir::Type;

    auto fresh_integral_type_variable(
        Inference_state& state, hir::Arena& arena, kieli::Range origin) -> hir::Type;

    auto fresh_mutability_variable(Inference_state& state, hir::Arena& arena, kieli::Range origin)
        -> hir::Mutability;

    void flatten_type(Context& context, Inference_state& state, hir::Type_variant& type);

    void set_solution(
        Context&            context,
        Inference_state&    state,
        Type_variable_data& variable_data,
        hir::Type_variant   solution);

    void set_solution(
        Context&                  context,
        Inference_state&          state,
        Mutability_variable_data& variable_data,
        hir::Mutability_variant   solution);

    void ensure_no_unsolved_variables(Context& context, Inference_state& state);

    auto collect_document(Context& context, kieli::Document_id document_id) -> hir::Environment_id;

    void resolve_environment(Context& context, hir::Environment_id environment_id);

    auto resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&;

    auto resolve_concept(Context& context, Concept_info& info) -> hir::Concept&;

    auto resolve_alias(Context& context, Alias_info& info) -> hir::Alias&;

    auto resolve_function_body(Context& context, Function_info& info) -> hir::Expression&;

    auto resolve_function_signature(Context& context, Function_info& info)
        -> hir::Function_signature&;

    // Resolve template parameters and register them in the given scope.
    auto resolve_template_parameters(
        Context&                        context,
        Inference_state&                state,
        hir::Scope_id                   scope_id,
        hir::Environment_id             environment_id,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        Context&                                    context,
        Inference_state&                            state,
        hir::Scope_id                               scope_id,
        hir::Environment_id                         environment_id,
        std::vector<hir::Template_parameter> const& parameters,
        std::vector<ast::Template_argument> const&  arguments)
        -> std::vector<hir::Template_argument>;

    auto resolve_mutability(
        Context&               context,
        hir::Scope_id          scope_id,
        ast::Mutability const& mutability) -> hir::Mutability;

    auto resolve_expression(
        Context&               context,
        Inference_state&       state,
        hir::Scope_id          scope_id,
        hir::Environment_id    environment_id,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        Context&            context,
        Inference_state&    state,
        hir::Scope_id       scope_id,
        hir::Environment_id environment_id,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        Context&            context,
        Inference_state&    state,
        hir::Scope_id       scope_id,
        hir::Environment_id environment_id,
        ast::Type const&    type) -> hir::Type;

    auto resolve_concept_reference(
        Context&            context,
        Inference_state&    state,
        hir::Scope_id       scope_id,
        hir::Environment_id environment_id,
        ast::Path const&    path) -> hir::Concept_id;

    void bind_mutability(Scope& scope, kieli::Identifier identifier, Mutability_bind bind);
    void bind_variable(Scope& scope, kieli::Identifier identifier, Variable_bind bind);
    void bind_type(Scope& scope, kieli::Identifier identifier, Type_bind bind);

    // Emit warnings for any unused bindings.
    void report_unused(kieli::Database& db, Scope& scope);

    auto find_mutability(Context& context, hir::Scope_id scope_id, kieli::Identifier identifier)
        -> Mutability_bind*;
    auto find_variable(Context& context, hir::Scope_id scope_id, kieli::Identifier identifier)
        -> Variable_bind*;
    auto find_type(Context& context, hir::Scope_id scope_id, kieli::Identifier identifier)
        -> Type_bind*;

    // Check whether a type variable with `tag` occurs in `type`.
    auto occurs_check(
        hir::Arena const& arena, hir::Type_variable_id id, hir::Type_variant const& type) -> bool;

    // Require that `sub` is equal to or a subtype of `super`.
    void require_subtype_relationship(
        Context&                 context,
        Inference_state&         state,
        kieli::Range             range,
        hir::Type_variant const& sub,
        hir::Type_variant const& super);

    // Get the HIR representation of the error type with `range`.
    auto error_type(Constants const& constants, kieli::Range range) -> hir::Type;

    // Get the HIR representation of an error expression with `range`.
    auto error_expression(Constants const& constants, kieli::Range range) -> hir::Expression;

    // Get the HIR representation of a unit tuple expression with `range`.
    auto unit_expression(Constants const& constants, kieli::Range range) -> hir::Expression;

    // Invoke `callback` with a temporary `Scope_id`.
    auto scope(Context& context, Scope scope, std::invocable<hir::Scope_id> auto const& callback)
    {
        auto scope_id = context.info.scopes.push(std::move(scope));
        auto result   = std::invoke(callback, scope_id);
        context.info.scopes.kill(scope_id);
        return result;
    }

    auto child_scope(
        Context&                                  context,
        hir::Scope_id const                       parent_id,
        std::invocable<hir::Scope_id> auto const& callback)
    {
        auto const document_id = context.info.scopes.index_vector[parent_id].document_id;
        auto       child       = Scope { .document_id = document_id, .parent_id = parent_id };
        return scope(context, std::move(child), callback);
    }

    void debug_display_environment(Context const& context, hir::Environment_id id);

} // namespace libresolve
