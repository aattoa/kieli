#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>


namespace {
    auto warn_about_unused_bindings_impl(
        resolution::Context  & context,
        auto                 & bindings,
        std::string_view const description) -> void
    {
        for (auto& [name, binding] : bindings) {
            if (binding.has_been_mentioned) continue;
            context.diagnostics().emit_simple_warning({
                .erroneous_view      = binding.source_view,
                .message             = "Unused local {}",
                .message_arguments   = fmt::make_format_args(description),
                .help_note           = "If this is intentional, prefix the {} with an underscore: _{}",
                .help_note_arguments = fmt::make_format_args(description, name),
            });
        }
    }

    auto add_binding_impl(
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
            if (!it->second.has_been_mentioned && context.diagnostics().warning_level() != utl::diagnostics::Level::suppress) {
                context.diagnostics().emit_warning({
                    .sections = utl::to_vector<utl::diagnostics::Text_section>({
                        {
                            .source_view = it->second.source_view,
                            .note        = "First declared here",
                        },
                        {
                            .source_view = binding.source_view,
                            .note        = "Later shadowed here",
                        }
                    }),
                    .message             = "Local {} shadows an unused local {}",
                    .message_arguments   = fmt::make_format_args(description, description),
                    .help_note           = "If this is intentional, prefix the first {} with an underscore: _{}",
                    .help_note_arguments = fmt::make_format_args(description, it->first),
                });
                it->second.has_been_mentioned = true; // Prevent a second warning about the same variable
            }
            bindings.emplace(it, identifier, std::forward<decltype(binding)>(binding));
        }
    }

    template <auto (resolution::Scope::*find)(compiler::Identifier)>
    auto find_impl(compiler::Identifier const identifier, auto& bindings, resolution::Scope* const parent) noexcept
        -> decltype(bindings.find(identifier))
    {
        if (auto* const binding = bindings.find(identifier))
            return binding;
        else
            return parent ? (parent->*find)(identifier) : nullptr;
    }
}


auto resolution::Scope::bind_variable(Context& context, compiler::Identifier const identifier, Variable_binding&& binding) -> void {
    add_binding_impl(context, variable_bindings.container(), identifier, binding, "variable");
}
auto resolution::Scope::bind_type(Context& context, compiler::Identifier const identifier, Type_binding&& binding) -> void {
    add_binding_impl(context, type_bindings.container(), identifier, binding, "type binding");
}
auto resolution::Scope::bind_mutability(Context& context, compiler::Identifier const identifier, Mutability_binding&& binding) -> void {
    add_binding_impl(context, mutability_bindings.container(), identifier, binding, "mutability binding");
}

auto resolution::Scope::find_variable(compiler::Identifier const identifier) noexcept -> Variable_binding* {
    return find_impl<&Scope::find_variable>(identifier, variable_bindings, parent);
}
auto resolution::Scope::find_type(compiler::Identifier const identifier) noexcept -> Type_binding* {
    return find_impl<&Scope::find_type>(identifier, type_bindings, parent);
}
auto resolution::Scope::find_mutability(compiler::Identifier const identifier) noexcept -> Mutability_binding* {
    return find_impl<&Scope::find_mutability>(identifier, mutability_bindings, parent);
}

auto resolution::Scope::make_child() noexcept -> Scope {
    Scope child; child.parent = this; return child;
}

auto resolution::Scope::warn_about_unused_bindings(Context& context) -> void {
    if (context.diagnostics().warning_level() == utl::diagnostics::Level::suppress) return;
    warn_about_unused_bindings_impl(context, variable_bindings.container(), "variable");
    warn_about_unused_bindings_impl(context, type_bindings.container(), "type binding");
    warn_about_unused_bindings_impl(context, mutability_bindings.container(), "mutability binding");
}
