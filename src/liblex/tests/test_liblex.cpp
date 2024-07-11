#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& string) -> Test_lex_result
{
    auto       db     = kieli::Database { .current_revision = 0 };
    auto const source = db.sources.push(std::move(string), "[test]");
    auto       state  = kieli::Lex_state::make(source, db);

    std::vector<kieli::Token> tokens;
    for (;;) {
        auto token = kieli::lex(state);
        if (token.type == kieli::Token_type::end_of_input) {
            break;
        }
        tokens.push_back(std::move(token));
    }

    return Test_lex_result {
        .formatted_tokens    = std::format("{}", utl::fmt::join(tokens, ", ")),
        .diagnostic_messages = kieli::format_diagnostics(db.diagnostics),
    };
}
