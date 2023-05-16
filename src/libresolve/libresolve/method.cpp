#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;


namespace {

    [[nodiscard]]
    auto is_implementation_for(
        Context&        context,
        mir::Type const self,
        mir::Type const inspected) -> bool
    {
        return context.pure_equality_compare(self, inspected);
    }


    struct [[nodiscard]] Method_lookup_result {
        utl::Wrapper<Function_info> method_info;
        std::variant<utl::Wrapper<Implementation_info>, utl::Wrapper<Implementation_template_info>> implementation_info;
    };


    auto lookup_method(
        Context&        context,
        ast::Name const method_name,
        mir::Type const inspected_type) -> Method_lookup_result
    {
        tl::optional<Method_lookup_result> return_value;

        auto const emit_ambiguity_error = [&](utl::Pair<utl::Source_view> const views) {
            context.diagnostics().emit_error({
                .sections = utl::to_vector<utl::diagnostics::Text_section>({
                    {
                        .source_view = method_name.source_view,
                        .note        = "Ambiguity here"
                    },
                    {
                        .source_view = views.first,
                        .note        = "Could be referring to this",
                        .note_color  = utl::diagnostics::warning_color
                    },
                    {
                        .source_view = views.second,
                        .note        = "or this",
                        .note_color  = utl::diagnostics::warning_color
                    }
                }),
                .message = "Ambiguous method: {}"_format(method_name),
            });
        };

        for (utl::wrapper auto const implementation_info : context.nameless_entities.implementations) {
            mir::Implementation& implementation = context.resolve_implementation(implementation_info);
            mir::Implementation::Definitions& definitions = implementation.definitions;

            // Try to find a method with the given name first and only then check if the implementation
            // concerns the given type, because the former is a much cheaper operation than the latter.

            if (utl::wrapper auto* const function = definitions.functions.find(method_name.identifier)) {
                if (is_implementation_for(context, implementation.self_type, inspected_type)) {
                    if (return_value.has_value())
                        emit_ambiguity_error({ return_value->method_info->name.source_view, (*function)->name.source_view });
                    else
                        return_value = Method_lookup_result { *function, implementation_info };
                }
            }
        }

        if (return_value.has_value())
            return *return_value;

        context.error(method_name.source_view, { "No appropriate method '{}' in scope"_format(method_name) });
    }

}


auto libresolve::Context::resolve_method(
    ast::Name                                              const method_name,
    tl::optional<std::span<hir::Template_argument const>> const template_arguments,
    mir::Type                                              const type,
    Scope                                                      & scope,
    Namespace                                                  & space) -> utl::Wrapper<Function_info>
{
    Method_lookup_result const lookup_result = lookup_method(*this, method_name, type);

    if (template_arguments.has_value())
        return instantiate_function_template(lookup_result.method_info, *template_arguments, method_name.source_view, scope, space);

    if (resolve_function_signature(*lookup_result.method_info).is_template())
        return instantiate_function_template_with_synthetic_arguments(lookup_result.method_info, method_name.source_view);
    else
        return lookup_result.method_info;
}
