#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libparse/parse.hpp>
#include <liblex/lex.hpp>
#include <libresolve/module.hpp>

namespace {

    constexpr auto message_format_for_read_error(utl::Source::Read_error const read_error)
        -> std::format_string<std::string_view>
    {
        switch (read_error) {
        case utl::Source::Read_error::does_not_exist:
            return "File '{}' does not exist";
        case utl::Source::Read_error::failed_to_open:
            return "Failed to open file '{}'";
        case utl::Source::Read_error::failed_to_read:
            return "Failed to read file '{}'";
        default:
            cpputil::unreachable();
        }
    }

    template <bool is_upper>
    auto environment_map_for(
        libresolve::Environment& environment, kieli::Basic_name<is_upper> const&) -> auto&
    {
        if constexpr (is_upper) {
            return environment.upper_map;
        }
        else {
            return environment.lower_map;
        }
    }

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

    template <class Info, bool is_upper>
    auto add_to_environment(
        libresolve::Context&                                context,
        utl::Source::Wrapper const                          source,
        utl::Mutable_wrapper<libresolve::Environment> const environment,
        kieli::Basic_name<is_upper> const                   name,
        decltype(Info::variant)&&                           variant) -> void
    {
        auto& map = environment_map_for(environment.as_mutable(), name);
        if (auto const* const definition = map.find(name.identifier)) {
            emit_duplicate_definitions_error(
                context.compile_info.diagnostics,
                source,
                definition->name.as_dynamic(),
                name.as_dynamic());
            return;
        }
        map.add_new_unchecked(
            name.identifier,
            std::conditional_t<is_upper, libresolve::Upper_info, libresolve::Lower_info> {
                name,
                source,
                context.info_arena.wrap<Info, utl::Wrapper_mutability::yes>(Info {
                    .variant     = std::move(variant),
                    .environment = environment,
                    .name        = name,
                }),
            });
    }

    auto add_definition_to_environment(
        libresolve::Context&                                context,
        ast::Definition&&                                   definition,
        utl::Mutable_wrapper<libresolve::Environment> const environment) -> void
    {
        utl::Source::Wrapper const source = definition.source;
        std::visit(
            utl::Overload {
                [&](ast::definition::Function&& function) {
                    add_to_environment<libresolve::Function_info>(
                        context, source, environment, function.signature.name, std::move(function));
                },
                [&](ast::definition::Struct&& structure) {
                    add_to_environment<libresolve::Structure_info>(
                        context, source, environment, structure.name, std::move(structure));
                },
                [&](ast::definition::Enum&& enumeration) {
                    add_to_environment<libresolve::Enumeration_info>(
                        context, source, environment, enumeration.name, std::move(enumeration));
                },
                [&](ast::definition::Alias&& alias) {
                    add_to_environment<libresolve::Alias_info>(
                        context, source, environment, alias.name, std::move(alias));
                },
                [&](ast::definition::Namespace&& space) {
                    add_to_environment<libresolve::Namespace_info>(
                        context, source, environment, space.name, std::move(space));
                },
                [&](ast::definition::Typeclass&& typeclass) {
                    add_to_environment<libresolve::Typeclass_info>(
                        context, source, environment, typeclass.name, std::move(typeclass));
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

    auto collect_environment(libresolve::Context& context, ast::Module&& ast)
        -> utl::Mutable_wrapper<libresolve::Environment>
    {
        context.ast_node_arena.merge_with(std::move(ast.node_arena));
        auto const environment = context.environment_arena.wrap<utl::Wrapper_mutability::yes>();
        for (ast::Definition& definition : ast.definitions) {
            add_definition_to_environment(context, std::move(definition), environment);
        }
        return environment;
    }

    auto recursively_read_module_to_module_map(
        libresolve::Context&         context,
        utl::Source::Wrapper const   importing_source,
        std::filesystem::path const& project_root,
        cst::Module::Import const    import,
        libresolve::Module_map&      module_map) -> void
    {
        auto file_path = (project_root / import.name.view()).replace_extension(".kieli");
        if (module_map.find(file_path)) {
            return;
        }
        auto& info = context.compile_info;
        if (auto imported_souce = utl::Source::read(std::move(file_path))) {
            auto const wrapped_source = info.source_arena.wrap(std::move(*imported_souce));
            auto const cst            = kieli::parse(wrapped_source, info);
            module_map.add_new_unchecked(
                wrapped_source->path(), collect_environment(context, kieli::desugar(cst, info)));
            for (auto const& import : cst.imports) {
                recursively_read_module_to_module_map(
                    context, importing_source, project_root, import, module_map);
            }
        }
        else {
            info.diagnostics.emit(
                cppdiag::Severity::error,
                importing_source,
                import.source_range,
                message_format_for_read_error(imported_souce.error()),
                import.name.view());
        }
    }

    // The nonexistent root module that recursively pulls in the project main and its dependencies.
    auto virtual_root_module(kieli::Compile_info& info) -> cst::Module
    {
        return kieli::parse(
            info.source_arena.wrap(
                std::filesystem::path("[kieli-internal-project-root]"), "import \"main\""),
            info);
    }

} // namespace

auto libresolve::read_module_map(Context& context, std::filesystem::path const& project_root)
    -> Module_map
{
    cst::Module root_module = virtual_root_module(context.compile_info);
    cpputil::always_assert(root_module.imports.size() == 1);

    Module_map module_map;
    recursively_read_module_to_module_map(
        context, root_module.source, project_root, root_module.imports.front(), module_map);
    return module_map;
}
