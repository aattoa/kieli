#include "utl/utilities.hpp"
#include "representation/cir/cir.hpp"
#include "representation/cir/cir_formatting.hpp"
#include "reify.hpp"
#include "reification_internals.hpp"


namespace {

    auto reify_function(reification::Context& context, resolution::Function_info& info) -> cir::Function {
        mir::Function& function = utl::get<mir::Function>(info.value);

        auto const reify_parameter_type = utl::compose(
            std::bind_front(&reification::Context::reify_type, &context), &mir::Function_parameter::type);

        return cir::Function {
            .symbol          = std::string(function.signature.name.identifier.view()), // TODO: format function signature
            .parameter_types = utl::map(reify_parameter_type, function.signature.parameters),
            .body            = context.reify_expression(function.body)
        };
    }

}


auto compiler::reify(Resolve_result&& resolve_result) -> Reify_result {
    reification::Context context { std::move(resolve_result.compilation_info), cir::Node_arena::with_default_page_size() };

    std::vector<cir::Function> functions;
    functions.reserve(resolve_result.module.functions.size());

    for (utl::wrapper auto const wrapped_function : resolve_result.module.functions)
        functions.push_back(reify_function(context, *wrapped_function));

    return Reify_result {
        .compilation_info = std::move(context.compilation_info),
        .node_arena       = std::move(context.node_arena),
        .functions        = std::move(functions),
    };
}