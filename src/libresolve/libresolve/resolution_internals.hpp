#pragma once

#include <libresolve/resolve.hpp>
#include <libresolve/module.hpp>
#include <libresolve/unification.hpp>

namespace libresolve {

    using Module_map = utl::Flatmap<std::filesystem::path, utl::Mutable_wrapper<Module_info>>;

    struct Context {
        Arenas                       arenas;
        Module_map                   module_map;
        Unification_state            unification_state;
        kieli::Project_configuration project_configuration;
        kieli::Compile_info&         compile_info;
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

    auto collect_environment(Context& context, std::vector<ast::Definition>&& definitions)
        -> Environment_wrapper;

    auto make_environment(Context& context, utl::Source::Wrapper source) -> Environment_wrapper;

    auto resolve_module(Context& context, Module_info& module_info) -> Environment_wrapper;

    auto resolve_definitions_in_order(Context& context, Environment_wrapper environment) -> void;

    auto resolve_definition(Context& context, Definition_variant const& definition) -> void;

    auto resolve_function(Context& context, Function_info& function) -> hir::Function&;

    auto resolve_enumeration(Context& context, Enumeration_info& enumeration) -> hir::Enumeration&;

    auto resolve_typeclass(Context& context, Typeclass_info& typeclass) -> hir::Typeclass&;

    auto resolve_alias(Context& context, Alias_info& alias) -> hir::Alias&;

    auto resolve_mutability(Context& context, ast::Mutability const& mutability) -> hir::Mutability;

    auto resolve_expression(Context& context, ast::Expression const& expression) -> hir::Expression;

    auto resolve_pattern(Context& context, ast::Pattern const& pattern) -> hir::Pattern;

    auto resolve_type(Context& context, ast::Type const& type) -> hir::Type;

} // namespace libresolve
