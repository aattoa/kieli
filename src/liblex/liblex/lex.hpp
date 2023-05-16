#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>
#include <liblex/token.hpp>


namespace kieli {

    struct [[nodiscard]] Lex_arguments {
        compiler::Compilation_info compilation_info = std::make_shared<compiler::Shared_compilation_info>();
        utl::Wrapper<utl::Source>  source;
    };

    struct [[nodiscard]] Lex_result {
        compiler::Compilation_info compilation_info;
        std::vector<Lexical_token> tokens;
    };

    auto lex(Lex_arguments&&) -> Lex_result;

}
