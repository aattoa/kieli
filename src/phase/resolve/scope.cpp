#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    auto warn_about_unused_bindings_impl(
        resolution::Context  & context,
        auto                 & bindings,
        std::string_view const description) -> void
    {
        for (auto& [name, binding] : bindings) {
            if (!binding.has_been_mentioned) {
                context.diagnostics.emit_simple_warning({
                    .erroneous_view      = binding.source_view,
                    .source              = context.source,
                    .message             = "Unused local {}",
                    .message_arguments   = fmt::make_format_args(description),
                    .help_note           = "If this is intentional, prefix the {} with an underscore: _{}",
                    .help_note_arguments = fmt::make_format_args(description, name)
                });
            }
        }
    }

    auto add_binding(
        resolution::Context      & context,
        auto                     & bindings,
        compiler::Identifier const identifier,
        auto                    && binding,
        std::string_view     const description)
    {
        // If the name starts with an underscore, then we pretend that the
        // binding has already been mentioned in order to prevent possible warnings.
        binding.has_been_mentioned = identifier.view().front() == '_';

        if (auto const it = ranges::find(bindings, identifier, utl::first); it == bindings.end()) {
            bindings.emplace_back(identifier, std::forward<decltype(binding)>(binding));
        }
        else {
            // The manual warning level check is not necessary because diagnostics.emit_warning performs the
            // same check internally, but this way the vector allocation can be skipped when warnings are suppressed.
            if (!it->second.has_been_mentioned && context.diagnostics.warning_level() != utl::diagnostics::Level::suppress) {
                context.diagnostics.emit_warning({
                    .sections = utl::to_vector<utl::diagnostics::Text_section>({
                        {
                            .source_view = it->second.source_view,
                            .source      = context.source,
                            .note        = "First declared here"
                        },
                        {
                            .source_view = binding.source_view,
                            .source      = context.source,
                            .note        = "Later shadowed here"
                        }
                    }),
                    .message             = "Local {} shadows an unused local {}",
                    .message_arguments   = fmt::make_format_args(description, description),
                    .help_note           = "If this is intentional, prefix the first {} with an underscore: _{}",
                    .help_note_arguments = fmt::make_format_args(description, it->first)
                });
                it->second.has_been_mentioned = true; // Prevent a second warning about the same variable
            }
            bindings.emplace(it, identifier, std::forward<decltype(binding)>(binding));
        }
    }

}


resolution::Scope::Scope(Context& context) noexcept
    : context { &context } {}


auto resolution::Scope::bind_variable(compiler::Identifier const identifier, Variable_binding&& binding) -> void {
    add_binding(*context, variable_bindings.container(), identifier, binding, "variable");
}
auto resolution::Scope::bind_type(compiler::Identifier const identifier, Type_binding&& binding) -> void {
    add_binding(*context, type_bindings.container(), identifier, binding, "type alias");
}
auto resolution::Scope::bind_mutability(compiler::Identifier const identifier, Mutability_binding&& binding) -> void {
    add_binding(*context, mutability_bindings.container(), identifier, binding, "mutability binding");
}


auto resolution::Scope::find_variable(compiler::Identifier const identifier) noexcept -> Variable_binding* {
    if (Variable_binding* const binding = variable_bindings.find(identifier))
        return binding;
    else
        return parent ? parent->find_variable(identifier) : nullptr;
}
auto resolution::Scope::find_type(compiler::Identifier const identifier) noexcept -> Type_binding* {
    if (Type_binding* const binding = type_bindings.find(identifier))
        return binding;
    else
        return parent ? parent->find_type(identifier) : nullptr;
}
auto resolution::Scope::find_mutability(compiler::Identifier const identifier) noexcept -> Mutability_binding* {
    if (Mutability_binding* const binding = mutability_bindings.find(identifier))
        return binding;
    else
        return parent ? parent->find_mutability(identifier) : nullptr;
}


auto resolution::Scope::make_child() noexcept -> Scope {
    Scope child { *context };
    child.parent = this;
    return child;
}


auto resolution::Scope::warn_about_unused_bindings() -> void {
    warn_about_unused_bindings_impl(*context, variable_bindings.container(), "variable");
    warn_about_unused_bindings_impl(*context, type_bindings.container(), "type alias");
    warn_about_unused_bindings_impl(*context, mutability_bindings.container(), "mutability binding");
}