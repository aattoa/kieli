#pragma once

#include "representation/hir/hir.hpp"
#include "representation/mir/mir.hpp"
#include "phase/desugar/desugar.hpp"


namespace compiler {

    struct Module_path {
        std::string period_separated_path_from_project_root; // TODO
    };

    struct Resolve_result {
        mir::Module                           main_module;
        //std::vector<Module_path, mir::Module> imports;

        mir::Node_context              node_context;
        mir::Namespace_context         namespace_context;
        compiler::Program_string_pool& string_pool;
    };

    auto resolve(Desugar_result&&) -> Resolve_result;

}
