#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    using namespace resolution;


    [[nodiscard]]
    auto is_implementation_for(
        Context&        context,
        mir::Type const self,
        mir::Type const inspected) -> bool
    {
        // TODO: treat template parameter references as variables to be matched against
        return context.pure_try_equate_types(self.value, inspected.value);
    }


    struct [[nodiscard]] Method_lookup_result {
        std::variant<utl::Wrapper<Function_info      >, utl::Wrapper<Function_template_info      >> method_info;
        std::variant<utl::Wrapper<Implementation_info>, utl::Wrapper<Implementation_template_info>> implementation_info;
    };


    auto lookup_method(
        Context&        context,
        ast::Name const method_name,
        mir::Type const inspected_type) -> Method_lookup_result
    {
        tl::optional<Method_lookup_result> return_value;

        auto const emit_ambiguity_error = [&](utl::Pair<utl::Source_view> const views) {
            context.diagnostics.emit_error({
                .sections = utl::to_vector<utl::diagnostics::Text_section>({
                    {
                        .source_view = method_name.source_view,
                        .source      = context.source,
                        .note        = "Ambiguity here"
                    },
                    {
                        .source_view = views.first,
                        .source      = context.source,
                        .note        = "Could be referring to this",
                        .note_color  = utl::diagnostics::warning_color
                    },
                    {
                        .source_view = views.second,
                        .source      = context.source,
                        .note        = "or this",
                        .note_color  = utl::diagnostics::warning_color
                    }
                }),
                .message           = "Ambiguous method: {}",
                .message_arguments = fmt::make_format_args(method_name)
            });
        };

        static constexpr auto get_source_view =
            [](utl::wrapper auto const info) { return info->name.source_view; };

        for (utl::wrapper auto const implementation_info : context.nameless_entities.implementations) {
            mir::Implementation& implementation = context.resolve_implementation(implementation_info);
            mir::Implementation::Definitions& definitions = implementation.definitions;

            auto const try_set_return_value = [&](utl::wrapper auto method) {
                if (is_implementation_for(context, implementation.self_type, inspected_type)) {
                    if (return_value.has_value())
                        emit_ambiguity_error({ std::visit(get_source_view, return_value->method_info), get_source_view(method) });
                    else
                        return_value = Method_lookup_result{ method, implementation_info };
                }
            };

            // Try to find a method with the given name first and only then check if the implementation
            // concerns the given type, because the former is a much cheaper operation than the latter.

            if (utl::wrapper auto* const function = definitions.functions.find(method_name.identifier))
                try_set_return_value(*function);
            else if (utl::wrapper auto* const function_template = definitions.function_templates.find(method_name.identifier))
                try_set_return_value(*function_template);
        }

        if (return_value.has_value())
            return *return_value;

        context.error(method_name.source_view, {
            .message           = "No appropriate method '{}' in scope",
            .message_arguments = fmt::make_format_args(method_name)
        });
    }

}


auto resolution::Context::resolve_method(
    ast::Name                                              const method_name,
    tl::optional<std::span<hir::Template_argument const>> const template_arguments,
    mir::Type                                              const type,
    Scope                                                      & scope,
    Namespace                                                  & space) -> utl::Wrapper<Function_info>
{
    Method_lookup_result lookup_result = lookup_method(*this, method_name, type);

    return utl::match(
        lookup_result.method_info,
        [&](utl::Wrapper<Function_info> const info) {
            if (template_arguments.has_value())
                error(method_name.source_view, { "This method is not a template, but template arguments were supplied" });
            else
                return info;
        },
        [&](utl::Wrapper<Function_template_info> const info) {
            if (template_arguments.has_value())
                return instantiate_function_template(info, *template_arguments, method_name.source_view, scope, space);
            else
                return instantiate_function_template_with_synthetic_arguments(info, method_name.source_view);
        }
    );
}
