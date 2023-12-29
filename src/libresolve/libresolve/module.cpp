#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libparse/parse.hpp>
#include <liblex/lex.hpp>
#include <libresolve/module.hpp>

namespace {

    auto message_format_for_read_error(utl::Source::Read_error const read_error)
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
            utl::unreachable();
        }
    }

    template <class Info, bool is_upper>
    auto add_to_environment(
        libresolve::Context&                                context,
        utl::Mutable_wrapper<libresolve::Environment> const environment,
        kieli::Basic_name<is_upper> const                   name,
        auto&&                                              ast) -> void
    {
        auto& map = std::invoke([environment] -> auto& {
            if constexpr (is_upper) {
                return environment.as_mutable().upper_map;
            }
            else {
                return environment.as_mutable().lower_map;
            }
        });
        map.add_new_or_abort(
            name.identifier,
            context.info_arena.wrap<Info, utl::Wrapper_mutability::yes>(Info {
                .variant     = std::forward<decltype(ast)>(ast),
                .environment = environment,
                .name        = name,
            }));
    }

    auto add_definition_to_environment(
        libresolve::Context&                                context,
        ast::Definition&&                                   definition,
        utl::Mutable_wrapper<libresolve::Environment> const environment) -> void
    {
        utl::match(
            std::move(definition.value),
            [&](ast::definition::Function&& function) {
                add_to_environment<libresolve::Function_info>(
                    context, environment, function.signature.name, std::move(function));
            },
            [&](ast::definition::Struct&& structure) {
                add_to_environment<libresolve::Structure_info>(
                    context, environment, structure.name, std::move(structure));
            },
            [&](ast::definition::Enum&& enumeration) {
                add_to_environment<libresolve::Enumeration_info>(
                    context, environment, enumeration.name, std::move(enumeration));
            },
            [&](ast::definition::Alias&& alias) {
                add_to_environment<libresolve::Alias_info>(
                    context, environment, alias.name, std::move(alias));
            },
            [&](ast::definition::Namespace&& space) {
                add_to_environment<libresolve::Namespace_info>(
                    context, environment, space.name, std::move(space));
            },
            [&](ast::definition::Typeclass&& typeclass) {
                add_to_environment<libresolve::Typeclass_info>(
                    context, environment, typeclass.name, std::move(typeclass));
            },
            [&](ast::definition::Implementation&& implementation) {
                (void)implementation;
                utl::todo();
            },
            [&](ast::definition::Instantiation&& instantiation) {
                (void)instantiation;
                utl::todo();
            });
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
        kieli::Compile_info&         info,
        libresolve::Context&         context,
        std::filesystem::path const& project_root,
        cst::Module::Import const    import,
        libresolve::Module_map&      module_map) -> void
    {
        auto file_path = (project_root / import.name.view()).replace_extension(".kieli");
        if (module_map.find(file_path)) {
            return;
        }
        if (auto source = utl::Source::read(std::move(file_path))) {
            auto const wrapped_source = info.source_arena.wrap(std::move(*source));
            auto const cst            = kieli::parse(kieli::lex(wrapped_source, info), info);
            module_map.add_new_unchecked(
                wrapped_source->path(), collect_environment(context, kieli::desugar(cst, info)));
            for (auto const& import : cst.imports) {
                recursively_read_module_to_module_map(
                    info, context, project_root, import, module_map);
            }
        }
        else {
            info.diagnostics.emit(
                cppdiag::Severity::error,
                import.source_view,
                message_format_for_read_error(source.error()),
                import.name.view());
        }
    }

    auto fake_main_import(kieli::Compile_info& info) -> cst::Module::Import
    {
        utl::Source::Wrapper const source = info.source_arena.wrap(
            std::filesystem::path("[kieli-internal-project-root]"), "import \"main\"");
        return kieli::parse(kieli::lex(source, info), info).imports.front();
    }

} // namespace

auto libresolve::read_module_map(
    kieli::Compile_info&         info,
    Context&                     context,
    std::filesystem::path const& project_root) -> Module_map
{
    Module_map module_map;
    recursively_read_module_to_module_map(
        info, context, project_root, fake_main_import(info), module_map);
    return module_map;
}
