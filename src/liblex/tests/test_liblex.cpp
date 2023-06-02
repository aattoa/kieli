#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"


auto liblex::test_lex(std::string&& string) -> Test_lex_result {
    compiler::Compilation_info test_info = compiler::mock_compilation_info();
    utl::wrapper auto const test_source = test_info.get()->source_arena.wrap("[test]", std::move(string));
    auto const lex_result = kieli::lex({ .compilation_info = std::move(test_info), .source = test_source});
    return {
        .formatted_tokens    = fmt::format("{}", lex_result.tokens),
        .diagnostic_messages = std::move(lex_result.compilation_info.get()->diagnostics).string(),
    };
}
