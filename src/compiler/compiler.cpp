#include "utl/utilities.hpp"
#include "utl/flatmap.hpp"
#include "utl/source.hpp"
#include "compiler.hpp"

#include "phase/parse/parse.hpp"
#include "phase/desugar/desugar.hpp"
#include "phase/resolve/resolve.hpp"


auto compiler::mock_compilation_info() -> Compilation_info {
    return std::make_shared<compiler::Shared_compilation_info>(compiler::Shared_compilation_info {
        .diagnostics = utl::diagnostics::Builder {
            utl::diagnostics::Builder::Configuration {
                .note_level    = utl::diagnostics::Level::suppress,
                .warning_level = utl::diagnostics::Level::suppress,
            }
        },
        .source_arena { /*page_size=*/ 1 },
    });
}

auto compiler::compile(Compile_arguments&&) -> Compilation_result {
    utl::todo();
}