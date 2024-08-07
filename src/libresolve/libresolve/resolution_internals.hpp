#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/disjoint_set.hpp>
#include <libresolve/resolve.hpp>
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

        static auto make_with(hir::Arena& arena) -> Constants;
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

    class Tag_state {
        std::size_t m_current_template_parameter_tag {};
        std::size_t m_current_local_variable_tag {};
    public:
        auto fresh_template_parameter_tag() -> hir::Template_parameter_tag;
        auto fresh_local_variable_tag() -> hir::Local_variable_tag;
    };

    struct Context {
        kieli::Database&             db;
        ast::Arena                   ast;
        hir::Arena                   hir;
        Info_arena                   info;
        Tag_state                    tags;
        Constants                    constants;
        kieli::Project_configuration configuration;
    };

    struct Inference_state {
        using Type_variables = utl::Index_vector<hir::Type_variable_id, Type_variable_data>;
        using Mutability_variables
            = utl::Index_vector<hir::Mutability_variable_id, Mutability_variable_data>;

        Type_variables       type_variables;
        Mutability_variables mutability_variables;
        utl::Disjoint_set    type_variable_disjoint_set;
        utl::Disjoint_set    mutability_variable_disjoint_set;
        kieli::Document_id   document_id;

        auto flatten(Context& context, hir::Type_variant& type) -> void;

        auto fresh_general_type_variable(hir::Arena& arena, kieli::Range origin) -> hir::Type;
        auto fresh_integral_type_variable(hir::Arena& arena, kieli::Range origin) -> hir::Type;
        auto fresh_mutability_variable(hir::Arena& arena, kieli::Range origin) -> hir::Mutability;

        auto set_solution(
            Context&            context,
            Type_variable_data& variable_data,
            hir::Type_variant   solution) -> void;

        auto set_solution(
            Context&                  context,
            Mutability_variable_data& variable_data,
            hir::Mutability_variant   solution) -> void;
    };

    struct Import_error {
        kieli::Lower erroneous_segment;
        bool         expected_module {};
    };

    auto resolve_import(
        kieli::Project_configuration const& configuration,
        std::span<kieli::Lower const>       path_segments) -> std::expected<Import, Import_error>;

    auto ensure_no_unsolved_variables(Context& context, Inference_state& state) -> void;

    auto add_to_environment(
        Context&            context,
        kieli::Document_id  document_id,
        Environment&        environment,
        kieli::Lower        name,
        Lower_info::Variant variant) -> void;

    auto add_to_environment(
        Context&            context,
        kieli::Document_id  document_id,
        Environment&        environment,
        kieli::Upper        name,
        Upper_info::Variant variant) -> void;

    auto collect_environment(
        Context&                       context,
        kieli::Document_id             document_id,
        std::vector<ast::Definition>&& definitions) -> hir::Environment_id;

    auto make_environment(Context& context, kieli::Document_id document_id) -> hir::Environment_id;

    auto resolve_module(Context& context, Module_info& module_info) -> hir::Environment_id;

    // Recursively resolve every definition in `environment`. Will not resolve function bodies.
    auto resolve_definitions_in_order(Context& context, hir::Environment_id environment) -> void;

    // Recursively resolve a definition. Will not resolve function bodies.
    auto resolve_definition(Context& context, Definition_variant const& definition) -> void;

    // Recursively resolve every function body in `environment`.
    auto resolve_function_bodies(Context& context, hir::Environment_id environment) -> void;

    auto resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&;

    auto resolve_concept(Context& context, Concept_info& info) -> hir::Concept&;

    auto resolve_alias(Context& context, Alias_info& info) -> hir::Alias&;

    auto resolve_function_body(Context& context, Function_info& info) -> hir::Function&;

    auto resolve_function_signature(Context& context, Function_info& info)
        -> hir::Function_signature&;

    // Resolve template parameters and register them in the given `scope`.
    auto resolve_template_parameters(
        Context&                        context,
        Inference_state&                state,
        Scope&                          scope,
        hir::Environment_id             environment,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        Context&                                    context,
        Inference_state&                            state,
        Scope&                                      scope,
        hir::Environment_id                         environment,
        std::vector<hir::Template_parameter> const& parameters,
        std::vector<ast::Template_argument> const&  arguments)
        -> std::vector<hir::Template_argument>;

    auto resolve_mutability(Context& context, Scope& scope, ast::Mutability const& mutability)
        -> hir::Mutability;

    auto resolve_expression(
        Context&               context,
        Inference_state&       state,
        Scope&                 scope,
        hir::Environment_id    environment,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        Context&            context,
        Inference_state&    state,
        Scope&              scope,
        hir::Environment_id environment,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        Context&            context,
        Inference_state&    state,
        Scope&              scope,
        hir::Environment_id environment,
        ast::Type const&    type) -> hir::Type;

    auto resolve_concept_reference(
        Context&                      context,
        Inference_state&              state,
        Scope&                        scope,
        hir::Environment_id           environment,
        ast::Concept_reference const& concept_reference) -> hir::Concept_reference;

    auto lookup_lower(
        Context&            context,
        Inference_state&    state,
        Scope&              scope,
        hir::Environment_id environment,
        ast::Path const&    path) -> std::optional<Lower_info>;

    auto lookup_upper(
        Context&            context,
        Inference_state&    state,
        Scope&              scope,
        hir::Environment_id environment,
        ast::Path const&    path) -> std::optional<Upper_info>;

    // Check whether a type variable with `tag` occurs in `type`.
    auto occurs_check(
        hir::Arena const& arena, hir::Type_variable_id id, hir::Type_variant const& type) -> bool;

    // Require that `sub` is equal to or a subtype of `super`.
    auto require_subtype_relationship(
        Context&                 context,
        Inference_state&         state,
        hir::Type_variant const& sub,
        hir::Type_variant const& super) -> void;

    auto error_expression(Constants const& constants, kieli::Range range) -> hir::Expression;
    auto unit_expression(Constants const& constants, kieli::Range range) -> hir::Expression;

} // namespace libresolve
