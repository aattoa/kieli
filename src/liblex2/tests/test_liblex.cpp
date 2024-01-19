#include <libutl/common/utilities.hpp>
#include <liblex2/lex.hpp>
#include "test_liblex.hpp"

auto liblex2::test_lex(std::string&& string) -> Test_lex_result
{
    auto [info, source] = kieli::test_info_and_source(std::move(string));

    kieli::Lex2_state state {
        .compile_info = info,
        .source       = source,
        .string       = source->string(),
    };

    std::vector<kieli::Token2> tokens;
    for (;;) {
        auto token = kieli::lex2(state);
        if (token.type == kieli::Token2::Type::end_of_input) {
            break;
        }
        tokens.push_back(std::move(token));
    }

    return Test_lex_result {
        .formatted_tokens    = std::format("{}", utl::fmt::join(tokens, ", ")),
        .diagnostic_messages = info.diagnostics.format_all({}),
    };
}
