#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libparse/parse.hpp>
#include <liblex/lex.hpp>
#include <libresolve/module.hpp>

namespace {

    auto read_error_message_format(utl::Source::Read_error const read_error)
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

    auto recursively_read_module_to_module_map(
        kieli::Compile_info&         info,
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
            module_map.add_new_unchecked(wrapped_source->path(), kieli::desugar(cst, info));
            for (auto const& import : cst.imports) {
                recursively_read_module_to_module_map(info, project_root, import, module_map);
            }
        }
        else {
            info.diagnostics.emit(
                cppdiag::Severity::error,
                import.source_view,
                read_error_message_format(source.error()),
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
    kieli::Compile_info& compile_info, std::filesystem::path const& project_root) -> Module_map
{
    Module_map module_map;
    recursively_read_module_to_module_map(
        compile_info, project_root, fake_main_import(compile_info), module_map);
    return module_map;
}
