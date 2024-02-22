#pragma once

#include <libresolve/resolve.hpp>
#include <libresolve/module.hpp>

namespace libresolve {

    struct Subtyping_constraint {
        hir::Type         subtype;
        hir::Type         supertype;
        utl::Source_range origin;
    };

    struct Type_equality_constraint {
        hir::Type         left;
        hir::Type         right;
        utl::Source_range origin;
    };

    using Module_map = utl::Flatmap<std::filesystem::path, utl::Mutable_wrapper<Module_info>>;

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
        utl::Mutable_wrapper<hir::Mutability::Variant> mutability;
        utl::Mutable_wrapper<hir::Mutability::Variant> immutability;

        static auto make_with(Arenas& arenas) -> Constants;
    };

    struct Obligation {
        hir::Type                            instance;
        utl::Mutable_wrapper<Typeclass_info> typeclass;
    };

    struct Unification_state {
        std::vector<hir::Type_variable_state>       type_states;
        std::vector<hir::Mutability_variable_state> mutability_states;

        auto fresh_general_type_variable(Arenas& arenas, utl::Source_range origin) -> hir::Type;
        auto fresh_integral_type_variable(Arenas& arenas, utl::Source_range origin) -> hir::Type;
        auto fresh_mutability_variable(Arenas& arenas, utl::Source_range origin) -> hir::Mutability;

        auto flatten(hir::Type type) const -> void;
        auto flatten(hir::Mutability mutability) const -> void;
    };

    struct Context {
        Arenas                              arenas;
        Constants                           constants;
        Module_map                          module_map;
        kieli::Project_configuration const& project_configuration;
        kieli::Compile_info&                compile_info;
    };

    struct Import_error {
        kieli::Name_lower erroneous_segment;
        bool              expected_module {};
    };

    auto resolve_import(
        kieli::Project_configuration const& configuration,
        std::span<kieli::Name_lower const>  path_segments) -> std::expected<Import, Import_error>;

    auto add_to_environment(
        Context&             context,
        utl::Source::Wrapper source,
        Environment_wrapper  environment,
        kieli::Name_lower    name,
        Lower_info::Variant  variant) -> void;

    auto add_to_environment(
        Context&             context,
        utl::Source::Wrapper source,
        Environment_wrapper  environment,
        kieli::Name_upper    name,
        Upper_info::Variant  variant) -> void;

    auto collect_environment(
        Context&                       context,
        utl::Source::Wrapper           source,
        std::vector<ast::Definition>&& definitions) -> Environment_wrapper;

    auto make_environment(Context& context, utl::Source::Wrapper source) -> Environment_wrapper;

    auto resolve_module(Context& context, Module_info& module_info) -> Environment_wrapper;

    auto resolve_definitions_in_order(Context& context, Environment_wrapper environment) -> void;

    auto resolve_definition(Context& context, Definition_variant const& definition) -> void;

    auto resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&;

    auto resolve_typeclass(Context& context, Typeclass_info& info) -> hir::Typeclass&;

    auto resolve_alias(Context& context, Alias_info& info) -> hir::Alias&;

    auto resolve_function(Context& context, Function_info& info) -> hir::Function&;

    auto resolve_function_signature(Context& context, Function_info& info)
        -> hir::Function::Signature&;

    auto resolve_mutability(
        Context&               context,
        Unification_state&     state,
        Scope&                 scope,
        ast::Mutability const& mutability) -> hir::Mutability;

    auto resolve_expression(
        Context&               context,
        Unification_state&     state,
        Scope&                 scope,
        Environment_wrapper    environment,
        ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(
        Context&            context,
        Unification_state&  state,
        Scope&              scope,
        Environment_wrapper environment,
        ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(
        Context&            context,
        Unification_state&  state,
        Scope&              scope,
        Environment_wrapper environment,
        ast::Type const&    type) -> hir::Type;

    auto lookup_lower(
        Context&                   context,
        Unification_state&         state,
        Scope&                     scope,
        Environment_wrapper        environment,
        ast::Qualified_name const& name) -> std::optional<Lower_info>;

    auto lookup_upper(
        Context&                   context,
        Unification_state&         state,
        Scope&                     scope,
        Environment_wrapper        environment,
        ast::Qualified_name const& name) -> std::optional<Upper_info>;

    auto occurs_check(Unification_state const& state, hir::Type_variable_tag tag, hir::Type type)
        -> bool;

    auto solve(Context& context, Subtyping_constraint const& constraint) -> void;

    auto error_expression(Constants const&, utl::Source_range) -> hir::Expression;
    auto unit_expression(Constants const&, utl::Source_range) -> hir::Expression;
    auto error_type(Constants const&, utl::Source_range) -> hir::Type;
    auto unit_type(Constants const&, utl::Source_range) -> hir::Type;

} // namespace libresolve
