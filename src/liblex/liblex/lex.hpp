#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>
#include <liblex/token.hpp>

namespace kieli {
    struct [[nodiscard]] Lex_arguments {
        Compilation_info          compilation_info = std::make_shared<Shared_compilation_info>();
        utl::Wrapper<utl::Source> source;
    };

    struct [[nodiscard]] Lex_result {
        Compilation_info           compilation_info;
        std::vector<Lexical_token> tokens;
    };

    auto lex(Lex_arguments&&) -> Lex_result;
} // namespace kieli
