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
            .parameter_types = utl::map(reify_parameter_type, function.signature.parameters),
            .body            = context.reify_expression(function.body)
        };
    }

}


auto compiler::reify(Resolve_result&& resolve_result) -> Reify_result {
    cir::Node_context node_context;
    reification::Context context { std::move(resolve_result.diagnostics), std::move(resolve_result.source) };

    for (utl::wrapper auto const function : resolve_result.main_module.functions) {
        fmt::println("function: {} = {}", function->name, reify_function(context, *function).body);
    }
    for (utl::wrapper auto const function_template : resolve_result.main_module.function_templates) {
        fmt::print("function template: {}\n", function_template->name);
        for (utl::wrapper auto const instantiation : utl::get<mir::Function_template>(function_template->value).instantiations) {
            fmt::print("instantiation: [{}]\n", utl::get(instantiation->template_instantiation_info).template_arguments);
            reify_function(context, *instantiation);
        }
    }

    utl::todo();
}