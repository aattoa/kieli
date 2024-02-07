#include <libutl/common/utilities.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/module.hpp>

namespace {

    auto emit_duplicate_definitions_error(
        kieli::Diagnostics&        diagnostics,
        utl::Source::Wrapper const source,
        kieli::Name_dynamic const  first,
        kieli::Name_dynamic const  second) -> void
    {
        diagnostics.emit(
            cppdiag::Severity::error,
            {
                kieli::Simple_text_section {
                    .source       = source,
                    .source_range = first.source_range,
                    .note         = "First defined here",
                    .severity     = cppdiag::Severity::information,
                },
                kieli::Simple_text_section {
                    .source       = source,
                    .source_range = second.source_range,
                    .note         = "Later defined here",
                },
            },
            "Duplicate definitions of '{}' in the same module",
            first.identifier);
    }

    template <bool is_upper>
    auto environment_map(libresolve::Environment& environment) -> auto&
    {
        if constexpr (is_upper) {
            return environment.upper_map;
        }
        else {
            return environment.lower_map;
        }
    }

    template <class Info, bool is_upper>
    auto add_to_environment(
        libresolve::Arenas&                                 arenas,
        kieli::Compile_info&                                compile_info,
        utl::Source::Wrapper const                          source,
        utl::Mutable_wrapper<libresolve::Environment> const environment,
        kieli::Basic_name<is_upper> const                   name,
        decltype(Info::variant)&&                           variant) -> void
    {
        auto& map = environment_map<is_upper>(environment.as_mutable());
        if (auto const* const definition = map.find(name.identifier)) {
            emit_duplicate_definitions_error(
                compile_info.diagnostics, source, definition->name.as_dynamic(), name.as_dynamic());
            return;
        }
        map.add_new_unchecked(
            name.identifier,
            std::conditional_t<is_upper, libresolve::Upper_info, libresolve::Lower_info> {
                name,
                source,
                arenas.info_arena.wrap<Info, utl::Wrapper_mutability::yes>(Info {
                    .variant     = std::move(variant),
                    .environment = environment,
                    .name        = name,
                }),
            });
    }

    auto add_definition_to_environment(
        libresolve::Arenas&                                 arenas,
        kieli::Compile_info&                                compile_info,
        ast::Definition&&                                   definition,
        utl::Mutable_wrapper<libresolve::Environment> const environment) -> void
    {
        utl::Source::Wrapper const source = definition.source;
        std::visit(
            utl::Overload {
                [&](ast::definition::Function&& function) {
                    add_to_environment<libresolve::Function_info>(
                        arenas,
                        compile_info,
                        source,
                        environment,
                        function.signature.name,
                        std::move(function));
                },
                [&](ast::definition::Enum&& enumeration) {
                    add_to_environment<libresolve::Enumeration_info>(
                        arenas,
                        compile_info,
                        source,
                        environment,
                        enumeration.name,
                        std::move(enumeration));
                },
                [&](ast::definition::Typeclass&& typeclass) {
                    add_to_environment<libresolve::Typeclass_info>(
                        arenas,
                        compile_info,
                        source,
                        environment,
                        typeclass.name,
                        std::move(typeclass));
                },
                [&](ast::definition::Alias&& alias) {
                    add_to_environment<libresolve::Alias_info>(
                        arenas, compile_info, source, environment, alias.name, std::move(alias));
                },
                [&](ast::definition::Namespace&& space) {
                    add_to_environment<libresolve::Namespace_info>(
                        arenas, compile_info, source, environment, space.name, std::move(space));
                },
                [&](ast::definition::Implementation&& implementation) {
                    (void)implementation;
                    cpputil::todo();
                },
                [&](ast::definition::Instantiation&& instantiation) {
                    (void)instantiation;
                    cpputil::todo();
                },
            },
            std::move(definition.value));
    }

    auto collect_environment(
        libresolve::Arenas&  arenas,
        kieli::Compile_info& compile_info,
        ast::Module&&        ast) -> utl::Mutable_wrapper<libresolve::Environment>
    {
        arenas.ast_node_arena.merge_with(std::move(ast.node_arena));
        auto const environment = arenas.environment_arena.wrap<utl::Wrapper_mutability::yes>();
        for (ast::Definition& definition : ast.definitions) {
            add_definition_to_environment(arenas, compile_info, std::move(definition), environment);
        }
        return environment;
    }

    [[noreturn]] auto report_import_error(
        kieli::Diagnostics&             diagnostics,
        utl::Source::Wrapper const      source,
        libresolve::Import_error const& error) -> void
    {
        diagnostics.error(
            source,
            error.segment.source_range,
            "No {} '{}' exists",
            error.expected_module ? "module" : "directory",
            error.segment);
    }

    auto recursively_read_module_to_module_map(
        libresolve::Arenas&          arenas,
        kieli::Compile_info&         compile_info,
        std::filesystem::path const& project_root,
        utl::Source::Wrapper const   imported_source,
        libresolve::Module_map&      module_map) -> void
    {
        cst::Module const module = kieli::parse(imported_source, compile_info);
        module_map.add_new_unchecked(
            imported_source->path(),
            collect_environment(arenas, compile_info, kieli::desugar(module, compile_info)));
        for (cst::Module::Import const& import : module.imports) {
            auto path = libresolve::resolve_import_path(project_root, import.segments.elements);
            if (!path.has_value()) {
                report_import_error(compile_info.diagnostics, imported_source, path.error());
            }
            else if (!module_map.find(path)) {
                recursively_read_module_to_module_map(
                    arenas,
                    compile_info,
                    project_root,
                    compile_info.source_arena.wrap(
                        utl::Source::read(std::move(path.value())).value()),
                    module_map);
            }
        }
    }

} // namespace

auto libresolve::resolve_import_path(
    std::filesystem::path const&             project_root_directory,
    std::span<kieli::Name_lower const> const path_segments)
    -> std::expected<std::filesystem::path, Import_error>
{
    cpputil::always_assert(!path_segments.empty());
    auto const middle_segments = path_segments.subspan(0, path_segments.size() - 1);
    auto const module_segment  = path_segments.back();

    auto path = project_root_directory;

    for (kieli::Name_lower const& segment : middle_segments) {
        path /= segment.identifier.string.view();
        if (!is_directory(path)) {
            return std::unexpected(libresolve::Import_error {
                .segment         = segment,
                .expected_module = false,
            });
        }
        // TODO: super segments
    }

    path /= std::format("{}.kieli", module_segment);
    if (is_regular_file(path)) {
        return path;
    }
    return std::unexpected(libresolve::Import_error {
        .segment         = module_segment,
        .expected_module = true,
    });
};

auto libresolve::read_module_map(
    Arenas&                      arenas,
    kieli::Compile_info&         compile_info,
    std::filesystem::path const& project_root) -> Module_map
{
    auto const main_file_path = project_root / "main.kieli";
    if (auto main_source = utl::Source::read(auto(main_file_path))) {
        Module_map module_map;
        recursively_read_module_to_module_map(
            arenas,
            compile_info,
            project_root,
            compile_info.source_arena.wrap(std::move(main_source.value())),
            module_map);
        return module_map;
    }
    compile_info.diagnostics.error("Project main file not found: '{}'", main_file_path.c_str());
}
