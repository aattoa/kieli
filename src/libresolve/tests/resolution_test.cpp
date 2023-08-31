#include <libutl/common/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>

#include "resolution_test.hpp"

namespace {
    auto format_hir_functions(std::span<hir::Function const> const functions) -> std::string
    {
        std::string output;
        for (hir::Function const& function : functions) {
            if (function.signature.is_template()) {
                continue;
            }
            hir::format_to(function, output);
        }
        return output;
    }
} // namespace

auto libresolve::do_test_resolve(std::string string) -> Do_test_resolve_result
{
    kieli::Compilation_info test_info
        = kieli::mock_compilation_info(utl::diagnostics::Level::normal);
    utl::wrapper auto const test_source
        = test_info.get()->source_arena.wrap("[test]", std::move(string));

    auto lex_result
        = kieli::lex({ .compilation_info = std::move(test_info), .source = test_source });
    auto resolve_result = resolve(desugar(parse(std::move(lex_result))));

    return Do_test_resolve_result {
        .formatted_hir_functions = format_hir_functions(resolve_result.functions),
        .diagnostics_messages
        = std::move(resolve_result.compilation_info.get()->diagnostics).string(),
    };
}
