#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/disjoint_set.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/module.hpp>

namespace libresolve {

    struct Constants {
        hir::Type_wrapper       i8_type;
        hir::Type_wrapper       i16_type;
        hir::Type_wrapper       i32_type;
        hir::Type_wrapper       i64_type;
        hir::Type_wrapper       u8_type;
        hir::Type_wrapper       u16_type;
        hir::Type_wrapper       u32_type;
        hir::Type_wrapper       u64_type;
        hir::Type_wrapper       boolean_type;
        hir::Type_wrapper       floating_type;
        hir::Type_wrapper       string_type;
        hir::Type_wrapper       character_type;
        hir::Type_wrapper       unit_type;
        hir::Type_wrapper       error_type;
        hir::Mutability_wrapper mutability_yes;
        hir::Mutability_wrapper mutability_no;
        hir::Mutability_wrapper mutability_error;

        static auto make_with(Arenas& arenas) -> Constants;
    };

    struct Obligation {
        hir::Type                            instance;
        utl::Mutable_wrapper<Typeclass_info> typeclass;
    };

    struct Type_variable_data {
        Type_variable_tag       tag;
        hir::Type_variable_kind kind {};
        utl::Source_range       origin;
        bool                    is_solved {};
        hir::Type_wrapper       variant;
    };

    struct Mutability_variable_data {
        Mutability_variable_tag tag;
        utl::Source_range       origin;
        bool                    is_solved {};
        hir::Mutability_wrapper variant;
    };

    struct Inference_state {
        using Type_variables = utl::Index_vector<Type_variable_tag, Type_variable_data>;
        using Mutability_variables
            = utl::Index_vector<Mutability_variable_tag, Mutability_variable_data>;

        Type_variables       type_variables;
        Mutability_variables mutability_variables;
        utl::Disjoint_set    type_variable_disjoint_set;
        utl::Disjoint_set    mutability_variable_disjoint_set;
        utl::Source_id       source;

        auto flatten(hir::Type::Variant& type) -> void;

        auto fresh_general_type_variable(Arenas& arenas, utl::Source_range origin) -> hir::Type;
        auto fresh_integral_type_variable(Arenas& arenas, utl::Source_range origin) -> hir::Type;
        auto fresh_mutability_variable(Arenas& arenas, utl::Source_range origin) -> hir::Mutability;

        auto set_solution(
            std::vector<cppdiag::Diagnostic>& diagnostics,
            Type_variable_data&               variable_data,
            hir::Type::Variant                solution) -> void;

        auto set_solution(
            std::vector<cppdiag::Diagnostic>& diagnostics,
            Mutability_variable_data&         variable_data,
            hir::Mutability::Variant          solution) -> void;
    };

    class Tag_state {
        std::size_t m_current_template_parameter_tag {};
        std::size_t m_current_local_variable_tag {};
    public:
        auto fresh_template_parameter_tag() -> Template_parameter_tag;
        auto fresh_local_variable_tag() -> Local_variable_tag;
    };

    using Module_map = utl::Flatmap<std::filesystem::path, utl::Mutable_wrapper<Module_info>>;

    struct Context {
        Arenas                              arenas;
        Constants                           constants;
        Module_map                          module_map;
        kieli::Project_configuration const& project_configuration;
        kieli::Compile_info&                compile_info;
        Tag_state                           tag_state;
    };

    struct Import_error {
        kieli::Name_lower erroneous_segment;
        bool              expected_module {};
    };

    auto resolve_import(
        kieli::Project_configuration const& configuration,
        std::span<kieli::Name_lower const>  path_segments) -> std::expected<Import, Import_error>;

    auto ensure_no_unsolved_variables(kieli::Compile_info& info, Inference_state& state) -> void;

    auto add_to_environment(
        Context&            context,
        utl::Source_id      source,
        Environment&        environment,
        kieli::Name_lower   name,
        Lower_info::Variant variant) -> void;

    auto add_to_environment(
        Context&            context,
        utl::Source_id      source,
        Environment&        environment,
        kieli::Name_upper   name,
        Upper_info::Variant variant) -> void;

    auto collect_environment(
        Context&                       context,
        utl::Source_id                 source,
        std::vector<ast::Definition>&& definitions) -> Environment_wrapper;

    auto make_environment(Context& context, utl::Source_id source) -> Environment_wrapper;

    auto resolve_module(Context& context, Module_info& module_info) -> Environment_wrapper;

    // Recursively resolve every definition in `environment`. Will not resolve function bodies.
    auto resolve_definitions_in_order(Context& context, Environment_wrapper environment) -> void;

    // Recursively resolve a definition. Will not resolve function bodies.
    auto resolve_definition(Context& context, Definition_variant const& definition) -> void;

    // Recursively resolve every function body in `environment`.
    auto resolve_function_bodies(Context& context, Environment_wrapper environment) -> void;

    auto resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&;

    auto resolve_typeclass(Context& context, Typeclass_info& info) -> hir::Typeclass&;

    auto resolve_alias(Context& context, Alias_info& info) -> hir::Alias&;

    auto resolve_function_body(Context& context, Function_info& info) -> hir::Function&;

    auto resolve_function_signature(Context& context, Function_info& info)
        -> hir::Function_signature&;

    // Resolve template parameters and register them in the given `scope`.
    auto resolve_template_parameters(
        Context&                        context,
        Inference_state&                state,
        Scope&                          scope,
        Environment_wrapper             environment,
        ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>;

    auto resolve_template_arguments(
        Context&                                    context,
        Inference_state&                            state,
        Scope&                                      scope,
        Environment_wrapper                         environment,
        std::vector<hir::Template_parameter> const& parameters,
        std::vector<ast::Template_argument> const&  arguments)
        -> std::vector<hir::Template_argument>;

    auto resolve_mutability(Context& context, Scope& scope, ast::Mutability const& mutability)
        -> hir::Mutability;

    auto resolve_expression(
        Context&               context,
        Inference_state&       state,
        Scope&                 scope,
        Environment_wrapper    environment,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        Context&            context,
        Inference_state&    state,
        Scope&              scope,
        Environment_wrapper environment,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        Context&            context,
        Inference_state&    state,
        Scope&              scope,
        Environment_wrapper environment,
        ast::Type const&    type) -> hir::Type;

    auto resolve_class_reference(
        Context&                    context,
        Inference_state&            state,
        Scope&                      scope,
        Environment_wrapper         environment,
        ast::Class_reference const& class_reference) -> hir::Class_reference;

    auto lookup_lower(
        Context&                   context,
        Inference_state&           state,
        Scope&                     scope,
        Environment_wrapper        environment,
        ast::Qualified_name const& name) -> std::optional<Lower_info>;

    auto lookup_upper(
        Context&                   context,
        Inference_state&           state,
        Scope&                     scope,
        Environment_wrapper        environment,
        ast::Qualified_name const& name) -> std::optional<Upper_info>;

    // Check whether a type variable with `tag` occurs in `type`.
    auto occurs_check(Type_variable_tag tag, hir::Type::Variant const& type) -> bool;

    // Require that `sub` is equal to or a subtype of `super`.
    auto require_subtype_relationship(
        std::vector<cppdiag::Diagnostic>& diagnostics,
        Inference_state&                  state,
        hir::Type::Variant const&         sub,
        hir::Type::Variant const&         super) -> void;

    auto error_expression(Constants const&, utl::Source_range) -> hir::Expression;
    auto unit_expression(Constants const&, utl::Source_range) -> hir::Expression;

} // namespace libresolve
