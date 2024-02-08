#pragma once

#include <libresolve/module.hpp>
#include <libresolve/unification.hpp>

namespace libresolve {

    using Module_map = utl::Flatmap<std::filesystem::path, utl::Mutable_wrapper<Module_info>>;

    struct Context {
        Arenas                arenas;
        Module_map            module_map;
        Unification_state     unification_state;
        std::filesystem::path project_root_directory;
        kieli::Compile_info&  compile_info;
    };

    struct Import_error {
        kieli::Name_lower erroneous_segment;
        bool              expected_module {};
    };

    auto resolve_import(
        std::filesystem::path const&       project_root_directory,
        std::span<kieli::Name_lower const> path_segments) -> std::expected<Import, Import_error>;

    auto resolve_module(Context& context, Module_info& module_info)
        -> utl::Mutable_wrapper<Environment>;

    auto collect_environment(Context& context, std::vector<ast::Definition>&& definitions)
        -> utl::Mutable_wrapper<libresolve::Environment>;

    [[noreturn]] auto report_duplicate_definitions_error(
        kieli::Diagnostics&  diagnostics,
        utl::Source::Wrapper source,
        kieli::Name_dynamic  first,
        kieli::Name_dynamic  second) -> void;

} // namespace libresolve
