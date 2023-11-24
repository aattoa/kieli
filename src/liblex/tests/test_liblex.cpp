#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& string) -> Test_lex_result
{
    auto [info, source] = kieli::test_info_and_source(std::move(string));
    auto const tokens   = kieli::lex(source, info);
    return Test_lex_result {
        .formatted_tokens    = std::format("{}", tokens),
        .diagnostic_messages = info.diagnostics.format_all({}),
    };
}
