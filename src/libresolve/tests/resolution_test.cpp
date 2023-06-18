#include <libutl/common/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>

#include "resolution_test.hpp"


namespace {
    auto format_mir_functions(std::span<mir::Function const>) -> std::string {
        utl::todo();
#if 0
        std::string formatted_mir_functions;
        for (mir::Function const& function : functions) {
            if (function.signature.is_template()) continue;
            std::format_to(std::back_inserter(formatted_mir_functions), "{}", function);
        }
        return formatted_mir_functions;
#endif
    }
}


auto libresolve::do_test_resolve(std::string string) -> Do_test_resolve_result {
    compiler::Compilation_info test_info = compiler::mock_compilation_info(utl::diagnostics::Level::normal);
    utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));

    auto lex_result = kieli::lex({ .compilation_info = std::move(test_info), .source = test_source });
    auto resolve_result = resolve(desugar(parse(std::move(lex_result))));

    return Do_test_resolve_result {
        .formatted_mir_functions = format_mir_functions(resolve_result.functions),
        .diagnostics_messages    = std::move(resolve_result.compilation_info.get()->diagnostics).string(),
    };
}
