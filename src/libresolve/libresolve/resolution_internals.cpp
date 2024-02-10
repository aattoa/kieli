#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::Arenas::defaults() -> Arenas
{
    return Arenas {
        .info_arena        = Info_arena::with_default_page_size(),
        .environment_arena = Environment_arena::with_default_page_size(),
        .ast_node_arena    = ast::Node_arena::with_default_page_size(),
        .hir_node_arena    = hir::Node_arena::with_default_page_size(),
    };
}
