#include <libutl/utilities.hpp>
#include <liblex/lex.hpp>
#include "test_liblex.hpp"

auto liblex::test_lex(std::string&& string) -> Test_lex_result
{
    auto       db          = kieli::Database {};
    auto const path        = std::filesystem::path("[test]");
    auto const document_id = kieli::add_document(db, path, std::move(string));
    auto       state       = kieli::lex_state(db, document_id);

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
        .diagnostic_messages = kieli::format_diagnostics(path, kieli::document(db, document_id)),
    };
}
