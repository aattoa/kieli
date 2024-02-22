#include <libutl/common/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& string) -> Test_lex_result
{
    auto [info, source] = kieli::test_info_and_source(std::move(string));
    auto state          = kieli::Lex_state::make(source, info);

    std::vector<kieli::Token> tokens;
    for (;;) {
        auto token = kieli::lex(state);
        if (token.type == kieli::Token::Type::end_of_input) {
            break;
        }
        tokens.push_back(std::move(token));
    }

    return Test_lex_result {
        .formatted_tokens    = std::format("{}", utl::fmt::join(tokens, ", ")),
        .diagnostic_messages = info.diagnostics.format_all({}),
    };
}
