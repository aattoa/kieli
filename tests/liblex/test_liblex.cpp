#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& text) -> Test_lex_result
{
    auto db    = kieli::Database {};
    auto id    = kieli::test_document(db, std::move(text));
    auto state = kieli::lex_state(db, id);

    std::vector<kieli::Token> tokens;
    for (;;) {
        kieli::Token token = kieli::lex(state);
        if (token.type == kieli::Token_type::end_of_input) {
            break;
        }
        tokens.push_back(token);
    }

    return Test_lex_result {
        .formatted_tokens    = std::format("{:n}", tokens),
        .diagnostic_messages = kieli::format_document_diagnostics(db, id),
    };
}
