#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    using namespace resolution;


    enum class Lookup_strategy {
        relative,
        absolute,
    };

    // TODO: improve
    auto namespace_name(Namespace const& space) -> std::string_view {
        return space.parent
            ? space.name ? space.name->identifier.view() : "<unnamed>"
            : "The global namespace";
    }


    auto apply_root_qualifier(
        Context            & context,
        Scope              & scope,
        Namespace          & space,
        hir::Root_qualifier& qualifier) -> utl::Pair<Lookup_strategy, Namespace*>
    {
        return utl::match(qualifier.value,
            [&](std::monostate) {
                return utl::Pair { Lookup_strategy::relative, &space };
            },
            [&](hir::Root_qualifier::Global) {
                return utl::Pair { Lookup_strategy::absolute, &*context.global_namespace };
            },
            [&](utl::Wrapper<hir::Type> type) {
                return utl::Pair {
                    Lookup_strategy::absolute,
                    &*context.associated_namespace(context.resolve_type(*type, scope, space))
                };
            }
        );
    }


    // Returns nullptr if the qualifier couldn't be applied
    auto apply_qualifier(
        Context       & context,
        Scope         & scope,
        Namespace     & space,
        hir::Qualifier& qualifier) -> Namespace*
    {
        if (qualifier.name.is_upper.get()) {
            if (auto* const item = space.upper_table.find(qualifier.name.identifier)) {
                auto const error_if_arguments = [&] {
                    if (qualifier.template_arguments.has_value())
                        context.error(qualifier.source_view, { "Template arguments applied to non-template entity" });
                };

                // TODO: simplify

                return utl::match(*item,
                    [&](utl::Wrapper<Struct_info> const info) -> Namespace* {
                        error_if_arguments();
                        return &*context.resolve_struct(info).associated_namespace;
                    },
                    [&](utl::Wrapper<Enum_info> const info) -> Namespace* {
                        error_if_arguments();
                        return &*context.resolve_enum(info).associated_namespace;
                    },
                    [&](utl::Wrapper<Alias_info> const info) -> Namespace* {
                        error_if_arguments();
                        return &*context.associated_namespace(
                            context.resolve_alias(info).aliased_type.with(qualifier.source_view)
                        );
                    },
                    [&](utl::Wrapper<Typeclass_info>) -> Namespace* {
                        error_if_arguments();
                        utl::todo();
                    },

                    [&](utl::Wrapper<Struct_template_info> const info) -> Namespace* {
                        return &*context.resolve_struct(
                            std::invoke([&] {
                                if (qualifier.template_arguments)
                                    return context.instantiate_struct_template(info, *qualifier.template_arguments, qualifier.source_view, scope, space);
                                else
                                    return context.instantiate_struct_template_with_synthetic_arguments(info, qualifier.source_view);
                            })
                        ).associated_namespace;
                    },
                    [&](utl::Wrapper<Enum_template_info> const info) -> Namespace* {
                        return &*context.resolve_enum(
                            std::invoke([&] {
                                if (qualifier.template_arguments)
                                    return context.instantiate_enum_template(info, *qualifier.template_arguments, qualifier.source_view, scope, space);
                                else
                                    return context.instantiate_enum_template_with_synthetic_arguments(info, qualifier.source_view);
                            })
                        ).associated_namespace;
                    },
                    [&](utl::Wrapper<Alias_template_info> const info) -> Namespace* {
                        return &*context.associated_namespace(
                            context.resolve_alias(
                                std::invoke([&] {
                                    if (qualifier.template_arguments)
                                        return context.instantiate_alias_template(info, *qualifier.template_arguments, qualifier.source_view, scope, space);
                                    else
                                        return context.instantiate_alias_template_with_synthetic_arguments(info, qualifier.source_view);
                                })
                            ).aliased_type
                        );
                    },
                    [](utl::Wrapper<Typeclass_template_info>) -> Namespace* {
                        utl::todo();
                    }
                );
            }
        }
        else {
            if (qualifier.template_arguments.has_value()) {
                utl::todo();
            }
            if (auto* const item = space.lower_table.find(qualifier.name.identifier)) {
                if (utl::wrapper auto* const child = std::get_if<utl::Wrapper<Namespace>>(item))
                    return &**child;
                else
                    context.error(qualifier.source_view, { "Expected a namespace" });
            }
        }

        return nullptr;
    }


    auto apply_relative_qualifier(
        Context       & context,
        Scope         & scope,
        Namespace     & space,
        hir::Qualifier& qualifier) -> Namespace*
    {
        Namespace* target = &space;

        for (;;) {
            if (Namespace* const new_space = apply_qualifier(context, scope, *target, qualifier)) {
                return new_space;
            }
            else if (target->parent) {
                target = &**target->parent;
            }
            else {
                context.error(qualifier.source_view, {
                    .message           = "{} does not contain a definition for this name",
                    .message_arguments = fmt::make_format_args(namespace_name(*target))
                });
            }
        }
    }


    auto apply_middle_qualifiers(
        Context                       & context,
        Scope                         & scope,
        Namespace                     & space,
        std::span<hir::Qualifier> const qualifiers) -> Namespace*
    {
        Namespace* target = &space;

        for (hir::Qualifier& qualifier : qualifiers) {
            if (Namespace* const new_target = apply_qualifier(context, scope, *target, qualifier)) {
                target = new_target;
            }
            else {
                context.error(qualifier.source_view, {
                    .message           = "{} contains no definition for this name",
                    .message_arguments = fmt::make_format_args(namespace_name(*target))
                });
            }
        }

        return target;
    }


    template <auto Namespace::* table>
    auto do_lookup(
        Context            & context,
        Scope              & scope,
        Namespace          & space,
        hir::Qualified_name& name) -> typename std::remove_cvref_t<decltype(space.*table)>::mapped_type
    {
        ast::Name const primary = name.primary_name;

        auto [lookup_strategy, root] = // NOLINT
            apply_root_qualifier(context, scope, space, name.root_qualifier);

        std::span<hir::Qualifier> qualifiers = name.middle_qualifiers;

        if (lookup_strategy == Lookup_strategy::relative) {
            if (qualifiers.empty()) {
                // Perform relative lookup with just the primary name

                for (;;) {
                    if (auto* const item = (root->*table).find(primary.identifier)) {
                        return *item;
                    }
                    else if (root->parent) {
                        root = &**root->parent;
                    }
                    else {
                        context.error(primary.source_view, {
                            .message           = "{} contains no definition for this name",
                            .message_arguments = fmt::make_format_args(namespace_name(space))
                        });
                    }
                }
            }
            else {
                // Perform relative lookup with the first qualifier

                root = apply_relative_qualifier(context, scope, *root, qualifiers.front());
                qualifiers = qualifiers.subspan<1>(); // Remove prefix
            }
        }

        Namespace* const target_space =
            apply_middle_qualifiers(context, scope, *root, qualifiers);

        if (utl::trivially_copyable auto const* const item = (target_space->*table).find(primary.identifier)) {
            return *item;
        }
        else {
            context.error(primary.source_view, {
                .message           = "{} contains no definition for this name",
                .message_arguments = fmt::make_format_args(namespace_name(*target_space))
            });
        }
    }

}


auto resolution::Context::find_lower(hir::Qualified_name& name, Scope& scope, Namespace& space) -> Lower_variant {
    assert(!name.primary_name.is_upper.get());
    return do_lookup<&Namespace::lower_table>(*this, scope, space, name);
}
auto resolution::Context::find_upper(hir::Qualified_name& name, Scope& scope, Namespace& space) -> Upper_variant {
    assert(name.primary_name.is_upper.get());
    return do_lookup<&Namespace::upper_table>(*this, scope, space, name);
}