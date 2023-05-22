#include <libutl/common/utilities.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


namespace {
    constexpr std::string_view source_string = R"(
        namespace std {
            class Copy { fn copy(&self): Self }
            class Drop { fn drop(&self): () }
            fn id[X](x: X) = x
        }
    )";
}


auto compiler::predefinitions_source(Compilation_info& compilation_info) -> utl::Wrapper<utl::Source> {
    return compilation_info.get()->source_arena.wrap("[predefined]", std::string(source_string));
}


auto compiler::mock_compilation_info(utl::diagnostics::Level const level) -> Compilation_info {
    return std::make_shared<compiler::Shared_compilation_info>(compiler::Shared_compilation_info {
        .diagnostics = utl::diagnostics::Builder {
            utl::diagnostics::Builder::Configuration { .note_level = level, .warning_level = level, }
        },
        .source_arena = utl::Source::Arena::with_page_size(1),
    });
}

