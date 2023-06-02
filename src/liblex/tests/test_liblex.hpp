#pragma once

#include <libutl/common/utilities.hpp>


namespace liblex {
    struct [[nodiscard]] Test_lex_result {
        std::string formatted_tokens;
        std::string diagnostic_messages;
    };
    auto test_lex(std::string&&) -> Test_lex_result;
}
