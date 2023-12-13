#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& string) -> Test_lex_result
{
    auto [info, source] = kieli::test_info_and_source(std::move(string));
    auto tokens         = kieli::lex(source, info);
    if (!tokens.empty() && tokens.back().type == kieli::Lexical_token::Type::end_of_input) {
        tokens.pop_back();
    }
    return Test_lex_result {
        .formatted_tokens    = std::format("{}", utl::fmt::join(tokens, ", ")),
        .diagnostic_messages = info.diagnostics.format_all({}),
    };
}
