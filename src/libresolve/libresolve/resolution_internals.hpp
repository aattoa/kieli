#pragma once

#include <libutl/common/index_vector.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/module.hpp>

namespace libresolve {

    struct Constants {
        utl::Mutable_wrapper<hir::Type::Variant>       i8_type;
        utl::Mutable_wrapper<hir::Type::Variant>       i16_type;
        utl::Mutable_wrapper<hir::Type::Variant>       i32_type;
        utl::Mutable_wrapper<hir::Type::Variant>       i64_type;
        utl::Mutable_wrapper<hir::Type::Variant>       u8_type;
        utl::Mutable_wrapper<hir::Type::Variant>       u16_type;
        utl::Mutable_wrapper<hir::Type::Variant>       u32_type;
        utl::Mutable_wrapper<hir::Type::Variant>       u64_type;
        utl::Mutable_wrapper<hir::Type::Variant>       boolean_type;
        utl::Mutable_wrapper<hir::Type::Variant>       floating_type;
        utl::Mutable_wrapper<hir::Type::Variant>       string_type;
        utl::Mutable_wrapper<hir::Type::Variant>       character_type;
        utl::Mutable_wrapper<hir::Type::Variant>       unit_type;
        utl::Mutable_wrapper<hir::Type::Variant>       error_type;
        utl::Mutable_wrapper<hir::Mutability::Variant> mutability_yes;
        utl::Mutable_wrapper<hir::Mutability::Variant> mutability_no;
        utl::Mutable_wrapper<hir::Mutability::Variant> mutability_error;

        static auto make_with(Arenas& arenas) -> Constants;
    };

    template <utl::vector_index Tag>
    using Variable_equalities = utl::Index_vector<Tag, std::vector<Tag>>;

    struct Unification_solutions {
        utl::Flatmap<Type_variable_tag, hir::Type> type_map;
        Variable_equalities<Type_variable_tag>     type_variable_equalities;

        utl::Flatmap<Mutability_variable_tag, hir::Mutability> mutability_map;
        Variable_equalities<Mutability_variable_tag>           mutability_variable_equalities;
    };

    struct Obligation {
        hir::Type                            instance;
        utl::Mutable_wrapper<Typeclass_info> typeclass;
        utl::Source_range                    origin;
    };

    struct Type_variable_data {
        Type_variable_tag                        tag;
        hir::Type_variable_kind                  kind {};
        utl::Source_range                        origin;
        utl::Mutable_wrapper<hir::Type::Variant> variant;
        bool                                     is_solved {};

        auto solve_with(hir::Type::Variant solution) -> void;
    };

    struct Mutability_variable_data {
        Mutability_variable_tag                        tag;
        utl::Source_range                              origin;
        utl::Mutable_wrapper<hir::Mutability::Variant> variant;
        bool                                           is_solved {};

        auto solve_with(hir::Mutability::Variant solution) -> void;
    };

    struct Inference_state {
        utl::Index_vector<Type_variable_tag, Type_variable_data>             type_variables;
        utl::Index_vector<Mutability_variable_tag, Mutability_variable_data> mutability_variables;
        utl::Source::Wrapper                                                 source;

        auto fresh_general_type_variable(Arenas& arenas, utl::Source_range origin) -> hir::Type;
        auto fresh_integral_type_variable(Arenas& arenas, utl::Source_range origin) -> hir::Type;
        auto fresh_mutability_variable(Arenas& arenas, utl::Source_range origin) -> hir::Mutability;
    };

    using Module_map = utl::Flatmap<std::filesystem::path, utl::Mutable_wrapper<Module_info>>;

    class Tag_state {
        std::size_t m_current_template_parameter_tag {};
        std::size_t m_current_local_variable_tag {};
    public:
        auto fresh_template_parameter_tag() -> Template_parameter_tag;
        auto fresh_local_variable_tag() -> Local_variable_tag;
    };

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

    auto ensure_no_unsolved_variables(Inference_state& state, kieli::Diagnostics& diagnostics)
        -> void;

    auto add_to_environment(
        Context&             context,
        utl::Source::Wrapper source,
        Environment&         environment,
        kieli::Name_lower    name,
        Lower_info::Variant  variant) -> void;

    auto add_to_environment(
        Context&             context,
        utl::Source::Wrapper source,
        Environment&         environment,
        kieli::Name_upper    name,
        Upper_info::Variant  variant) -> void;

    auto collect_environment(
        Context&                       context,
        utl::Source::Wrapper           source,
        std::vector<ast::Definition>&& definitions) -> Environment_wrapper;

    auto make_environment(Context& context, utl::Source::Wrapper source) -> Environment_wrapper;

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
    auto occurs_check(Type_variable_tag tag, hir::Type type) -> bool;

    // Require that `sub` is equal to or a subtype of `super`.
    auto require_subtype_relationship(
        kieli::Diagnostics& diagnostics,
        Inference_state&    state,
        hir::Type           sub,
        hir::Type           super,
        utl::Source_range   origin) -> void;

    auto error_expression(Constants const&, utl::Source_range) -> hir::Expression;
    auto unit_expression(Constants const&, utl::Source_range) -> hir::Expression;
    auto error_type(Constants const&, utl::Source_range) -> hir::Type;
    auto unit_type(Constants const&, utl::Source_range) -> hir::Type;

} // namespace libresolve
