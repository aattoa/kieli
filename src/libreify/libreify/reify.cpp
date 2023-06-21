#include <libutl/common/utilities.hpp>
#include <libreify/reification_internals.hpp>
#include <libreify/reify.hpp>


namespace {
    auto reify_function(libreify::Context& context, hir::Function const& function) -> cir::Function {
        auto const reify_parameter_type = utl::compose(
            std::bind_front(&libreify::Context::reify_type, &context), &hir::Function_parameter::type);
        return cir::Function {
            .symbol          = std::string(function.signature.name.identifier.view()), // TODO: format function signature
            .parameter_types = utl::map(reify_parameter_type, function.signature.parameters),
            .body            = context.reify_expression(function.body)
        };
    }
}


auto kieli::reify(Resolve_result&& resolve_result) -> Reify_result {
    libreify::Context context {
        std::move(resolve_result.compilation_info),
        cir::Node_arena::with_default_page_size(),
    };

    std::vector<cir::Function> functions = utl::map(
        std::bind_front(&reify_function, std::ref(context)),
        resolve_result.functions);

    return Reify_result {
        .compilation_info = std::move(context.compilation_info),
        .node_arena       = std::move(context.node_arena),
        .functions        = std::move(functions),
    };
}
