#include "utl/utilities.hpp"
#include "resolution.hpp"
#include "resolution_internals.hpp"
#include "lir/lir.hpp"


namespace {

    using namespace resolution;


    auto register_namespace(
        Context                        & context,
        std::span<hir::Definition> const definitions,
        utl::Wrapper<Namespace>           space) -> void
    {
        space->definitions_in_order.reserve(definitions.size());

        static_assert(utl::variant_has_alternative<Lower_variant, utl::Wrapper<Function_info>>);

        auto const add_definition = [&](utl::wrapper auto definition) -> void {
            if constexpr (utl::variant_has_alternative<Upper_variant, decltype(definition)>)
                context.add_to_namespace(*space, definition->name, definition);
            else if constexpr (utl::variant_has_alternative<Lower_variant, decltype(definition)>)
                context.add_to_namespace(*space, definition->name, definition);
            else
                static_assert(utl::always_false<decltype(definition)>);

            space->definitions_in_order.push_back(definition);
        };

        for (hir::Definition& definition : definitions) {
            utl::match(
                definition.value,

                [&](hir::definition::Function& function) {
                    ast::Name const name = function.name;
                    auto const info = utl::wrap(Function_info {
                        .value          = std::move(function),
                        .home_namespace = space,
                        .name           = name
                    });
                    context.output_module.functions.push_back(info);
                    add_definition(info);
                },
                [&](hir::definition::Alias& alias) {
                    ast::Name const name = alias.name;
                    add_definition(utl::wrap(Alias_info {
                        .value          = std::move(alias),
                        .home_namespace = space,
                        .name           = name
                    }));
                },
                [&](hir::definition::Typeclass& typeclass) {
                    ast::Name const name = typeclass.name;
                    add_definition(utl::wrap(resolution::Typeclass_info {
                        .value          = std::move(typeclass),
                        .home_namespace = space,
                        .name           = name
                    }));
                },
                [&](hir::definition::Struct& structure) {
                    mir::Type const structure_type =
                        context.temporary_placeholder_type(structure.name.source_view);
                    ast::Name const name = structure.name;

                    auto info = utl::wrap(Struct_info {
                        .value          = std::move(structure),
                        .home_namespace = space,
                        .structure_type = structure_type,
                        .name           = name
                    });
                    *structure_type.value = mir::type::Structure { info };
                    add_definition(info);
                },
                [&](hir::definition::Enum& enumeration) {
                    mir::Type const enumeration_type =
                        context.temporary_placeholder_type(enumeration.name.source_view);
                    ast::Name const name = enumeration.name;

                    auto info = utl::wrap(Enum_info{
                        .value            = std::move(enumeration),
                        .home_namespace   = space,
                        .enumeration_type = enumeration_type,
                        .name             = name
                    });
                    *enumeration_type.value = mir::type::Enumeration { info };
                    add_definition(info);
                },

                [&](hir::definition::Namespace& hir_child) {
                    utl::wrapper auto child = utl::wrap(Namespace {
                        .parent = space,
                        .name   = hir_child.name
                    });

                    space->definitions_in_order.push_back(child);
                    space->lower_table.add(utl::copy(hir_child.name.identifier), utl::copy(child));

                    register_namespace(context, hir_child.definitions, child);
                },

                [&](hir::definition::Function_template& template_definition) {
                    ast::Name const name = template_definition.definition.name;
                    auto const info = utl::wrap(Function_template_info {
                        .value = std::move(template_definition),
                        .home_namespace = space,
                        .name = name
                    });
                    context.output_module.function_templates.push_back(info);
                    add_definition(info);
                },

                [&]<class T>(ast::definition::Template<T>&template_definition) {
                    ast::Name const name = template_definition.definition.name;
                    add_definition(utl::wrap(Definition_info<ast::definition::Template<T>> {
                        .value                      = std::move(template_definition),
                        .home_namespace             = space,
                        .parameterized_type_of_this = context.temporary_placeholder_type(name.source_view),
                        .name                       = name
                    }));
                },
                    
                // TODO: reduce impl/inst duplication

                [&](hir::definition::Implementation& implementation) {
                    utl::wrapper auto const info = utl::wrap(Implementation_info {
                        .value          = std::move(implementation),
                        .home_namespace = space
                    });
                    space->definitions_in_order.push_back(info);
                    context.nameless_entities.implementations.push_back(info);
                },
                [&](hir::definition::Instantiation& instantiation) {
                    utl::wrapper auto const info = utl::wrap(Instantiation_info {
                        .value          = std::move(instantiation),
                        .home_namespace = space
                    });
                    space->definitions_in_order.push_back(info);
                    context.nameless_entities.instantiations.push_back(info);
                },
                [&](hir::definition::Implementation_template& implementation_template) {
                    utl::wrapper auto const info = utl::wrap(Implementation_template_info {
                        .value          = std::move(implementation_template),
                        .home_namespace = space
                    });
                    space->definitions_in_order.push_back(info);
                    context.nameless_entities.implementation_templates.push_back(info);
                },
                [&](hir::definition::Instantiation_template& instantiation_template) {
                    utl::wrapper auto const info = utl::wrap(Instantiation_template_info {
                        .value          = std::move(instantiation_template),
                        .home_namespace = space
                    });
                    space->definitions_in_order.push_back(info);
                    context.nameless_entities.instantiation_templates.push_back(info);
                },

                [](hir::definition::Namespace_template&) {
                    utl::todo();
                }
            );
        }
    }


    // Creates a resolution context by collecting top level name information
    auto register_top_level_definitions(compiler::Lower_result&& lower_result) -> Context {
        Context context {
            std::move(lower_result.node_context),
            mir::Node_context {
                utl::Wrapper_context<mir::Expression>          { lower_result.node_context.arena_size<hir::Expression>() },
                utl::Wrapper_context<mir::Pattern>             { lower_result.node_context.arena_size<hir::Pattern>   () },
                utl::Wrapper_context<mir::Type::Variant>       { lower_result.node_context.arena_size<hir::Expression>() + lower_result.node_context.arena_size<hir::Type>() },
                utl::Wrapper_context<mir::Mutability::Variant> { 1_uz /* Difficult to estimate, so just make sure at least one page is allocated */ }
            },
            mir::Namespace_context {},
            std::move(lower_result.diagnostics),
            std::move(lower_result.source),
            lower_result.string_pool
        };
        register_namespace(context, lower_result.module.definitions, context.global_namespace);
        return context;
    }


    // Resolves all definitions in order, but only visits function bodies if their return types have been omitted
    auto resolve_signatures(Context& context, utl::Wrapper<Namespace> space) -> void {
        for (Definition_variant& definition : space->definitions_in_order) {
            utl::match(
                definition,

                [&](utl::Wrapper<Function_info> info) {
                    (void)context.resolve_function_signature(*info);
                },
                [&](utl::Wrapper<Struct_info> info) {
                    (void)context.resolve_struct(info);
                },
                [&](utl::Wrapper<Enum_info> info) {
                    (void)context.resolve_enum(info);
                },
                [&](utl::Wrapper<Alias_info> info) {
                    (void)context.resolve_alias(info);
                },
                [&](utl::Wrapper<Typeclass_info> info) {
                    (void)context.resolve_typeclass(info);
                },
                [&](utl::Wrapper<Namespace> child) {
                    resolve_signatures(context, child);
                },
                [&](utl::Wrapper<Implementation_info> info) {
                    (void)context.resolve_implementation(info);
                },
                [&](utl::Wrapper<Instantiation_info> info) {
                    (void)context.resolve_instantiation(info);
                },

                [&](utl::Wrapper<Function_template_info> info) {
                    (void)context.resolve_function_template(info);
                },
                [&](utl::Wrapper<Struct_template_info> info) {
                    (void)context.resolve_struct_template(info);
                },
                [&](utl::Wrapper<Enum_template_info> info) {
                    (void)context.resolve_enum_template(info);
                },
                [&](utl::Wrapper<Alias_template_info> info) {
                    (void)context.resolve_alias_template(info);
                },
                [&](utl::Wrapper<Typeclass_template_info>) {
                    utl::todo();
                },
                [&](utl::Wrapper<Implementation_template_info> info) {
                    (void)context.resolve_implementation_template(info);
                },
                [&](utl::Wrapper<Instantiation_template_info> info) {
                    (void)context.resolve_instantiation_template(info);
                }
            );

            // Ensure function signatures are fully typechecked
            context.solve_deferred_constraints();
        }
    }


    // Resolves the remaining unresolved function bodies
    auto resolve_functions(Context& context, utl::Wrapper<Namespace> space) -> void {
        for (Definition_variant& definition : space->definitions_in_order) {
            if (utl::wrapper auto* const info = std::get_if<utl::Wrapper<Function_info>>(&definition)) {
                (void)context.resolve_function(*info);
                context.solve_deferred_constraints();
            }
            else if (utl::wrapper auto* const child = std::get_if<utl::Wrapper<Namespace>>(&definition)) {
                resolve_functions(context, *child);
            }
        }
    }

}


auto compiler::resolve(Lower_result&& lower_result) -> Resolve_result {
    Context context = register_top_level_definitions(std::move(lower_result));
    resolve_signatures(context, context.global_namespace);
    resolve_functions(context, context.global_namespace);
    context.solve_deferred_constraints();

#if 0
    [](this auto const visitor, Namespace const& space) -> void {
        for (auto& definition : space.definitions_in_order) {
            utl::match(
                definition,

                [](utl::Wrapper<Function_info> const def) {
                    fmt::print("{}\n\n", utl::get<2>(def->value));
                },
                [](utl::wrapper auto const def) {
                    fmt::print("{}\n\n", utl::get<1>(def->value));
                },
                [&](utl::Wrapper<Namespace> const space) {
                    fmt::print("namespace {} {{\n\n", space->name);
                    visitor(*space);
                    fmt::print("}} // closes {}\n\n", space->name);
                },
                [](utl::Wrapper<Function_template_info> const info) {
                    fmt::print(
                        "template [{}] {}\n\n",
                        utl::get<2>(info->value).parameters,
                        utl::get<2>(info->value).definition
                    );
                },
                []<class T>(utl::Wrapper<resolution::Definition_info<ast::definition::Template<T>>> const info) {
                    fmt::print(
                        "template [{}] {}\n\n",
                        utl::get<1>(info->value).parameters,
                        utl::get<1>(info->value).definition
                    );
                }
            );
        }
    }(*context.global_namespace);
#endif

    return Resolve_result {
        .main_module       = std::move(context.output_module),
        .node_context      = std::move(context.mir_node_context),
        .namespace_context = std::move(context.namespace_context),
        .string_pool       = context.string_pool
    };
}