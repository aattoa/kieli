#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libutl/source/source.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>
#include <compiler/compiler.hpp>


using Module_map = utl::Flatmap<std::filesystem::path, kieli::Desugar_result>;

namespace {
    auto read_modules_to(
        Module_map                               & module_map,
        std::filesystem::path               const& source_directory,
        compiler::Compilation_info          const& compilation_info,
        std::span<utl::Pooled_string const> const  imports) -> void
    {
        for (utl::Pooled_string const import : imports) {
            auto path = source_directory / import.view();
            if (module_map.find(path))
                continue;

            auto parse_result = kieli::parse(kieli::lex({
                .compilation_info = compilation_info,
                .source = compilation_info.get()->source_arena.wrap(utl::Source::read(std::filesystem::path { path })),
            }));
            read_modules_to(module_map, source_directory, compilation_info, parse_result.module.imports);
            module_map.add_new_unchecked(std::move(path), kieli::desugar(std::move(parse_result)));
        }
    }
}


auto compiler::compile(Compile_arguments&& compile_arguments) -> Compilation_result {
    Compilation_info compilation_info = std::make_shared<Shared_compilation_info>();

    utl::Pooled_string const main_file_name =
        compilation_info.get()->string_literal_pool.make(compile_arguments.main_file_name);

    Module_map module_map;
    read_modules_to(
        module_map,
        compile_arguments.source_directory_path,
        compilation_info,
        { &main_file_name, 1 });

    kieli::Desugar_result combined_desugar_result {
        .compilation_info = compilation_info,
        .node_arena = hir::Node_arena::with_default_page_size(),
    };

    combined_desugar_result.module.definitions.reserve(
        ranges::fold_left(
            module_map.container()
                | ranges::views::transform([](auto& x) { return x.second.module.definitions.size(); }),
            0_uz, std::plus {}));

    for (auto& [_, desugar_result] : module_map) { // NOLINT
        combined_desugar_result.node_arena.merge_with(std::move(desugar_result.node_arena));
        utl::append_vector(combined_desugar_result.module.definitions, std::move(desugar_result.module.definitions));
    }

    (void)resolve(std::move(combined_desugar_result));
    utl::todo();
}
