#pragma once

#include "phase/reify/reify.hpp"
#include "representation/cir/cir.hpp"
#include "representation/lir/lir.hpp"


namespace compiler {

    struct Lower_result {
        utl::diagnostics::Builder  diagnostics;
        utl::Source                source;
        lir::Node_context          node_context;
        std::vector<lir::Function> functions;
    };

    auto lower(Reify_result&&) -> Lower_result;

}