#include "utl/utilities.hpp"
#include "utl/flatmap.hpp"
#include "utl/source.hpp"
#include "compiler.hpp"

#include "phase/parse/parse.hpp"
#include "phase/desugar/desugar.hpp"
#include "phase/resolve/resolve.hpp"


using Module_map = utl::Flatmap<std::filesystem::path, compiler::Desugar_result>;

namespace {
    auto read_modules_to(
        Module_map&                             module_map,
        std::filesystem::path const&            source_directory,
        compiler::Compilation_info const&       compilation_info,
        std::span<compiler::String const> const imports) -> void
    {
        for (compiler::String const import : imports) {
            auto path = source_directory / import.view();
            if (module_map.find(path))
                continue;

            auto parse_result = compiler::parse(compiler::lex({
                .compilation_info = compilation_info,
                .source = compilation_info.get()->source_arena.wrap(utl::Source::read(std::filesystem::path { path })),
            }));
            read_modules_to(module_map, source_directory, compilation_info, parse_result.module.imports);
            module_map.add_new_unchecked(std::move(path), compiler::desugar(std::move(parse_result)));
        }
    }
}


auto compiler::mock_compilation_info() -> Compilation_info {
    return std::make_shared<compiler::Shared_compilation_info>(compiler::Shared_compilation_info {
        .diagnostics = utl::diagnostics::Builder {
            utl::diagnostics::Builder::Configuration {
                .note_level    = utl::diagnostics::Level::suppress,
                .warning_level = utl::diagnostics::Level::suppress,
            }
        },
        .source_arena = utl::Wrapper_arena<utl::Source>::with_page_size(1)
    });
}

auto compiler::compile(Compile_arguments&& compile_arguments) -> Compilation_result {
    Compilation_info compilation_info = std::make_shared<Shared_compilation_info>();

    compiler::String const main_file_name =
        compilation_info.get()->string_literal_pool.make(compile_arguments.main_file_name);

    Module_map module_map;
    read_modules_to(
        module_map,
        compile_arguments.source_directory_path,
        compilation_info,
        { &main_file_name, 1 });

    utl::todo();
}