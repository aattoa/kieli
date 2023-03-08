#pragma once

#include "hir/hir.hpp"
#include "mir/mir.hpp"
#include "ast/lower/lower.hpp"


namespace compiler {

    struct Module_path {
        std::string period_separated_path_from_project_root; // TODO
    };

    struct Resolve_result {
        mir::Module                           main_module;
        utl::Flatmap<Module_path, mir::Module> imports;

        mir::Node_context              node_context;
        mir::Namespace_context         namespace_context;
        compiler::Program_string_pool& string_pool;
    };

    auto resolve(Lower_result&&) -> Resolve_result;

}