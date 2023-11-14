#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& string) -> Test_lex_result
{
    auto [info, source]   = kieli::test_info_and_source(std::move(string));
    auto const lex_result = kieli::lex({ .compilation_info = std::move(info), .source = source });
    return {
        .formatted_tokens = std::format("{}", lex_result.tokens),
        .diagnostic_messages
        = std::move(lex_result.compilation_info.get()->old_diagnostics).string(),
    };
}
