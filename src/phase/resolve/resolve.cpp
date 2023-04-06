#include "utl/utilities.hpp"
#include "resolve.hpp"
#include "resolution_internals.hpp"
#include "representation/lir/lir.hpp"


namespace {

    using namespace resolution;


    // Collect top-level name information
    auto register_namespace(
        Context                        & context,
        std::span<hir::Definition> const definitions,
        utl::Wrapper<Namespace>           space) -> void
    {
        space->definitions_in_order.reserve(definitions.size());

        auto const add_definition = [&](utl::wrapper auto definition) -> void {
            context.add_to_namespace(*space, definition->name, definition);
            space->definitions_in_order.push_back(definition);
        };

        for (hir::Definition& definition : definitions) {
            utl::match(definition.value,
                [&](hir::definition::Function& function) {
                    // compiler::desugar should convert all function bodies to block form
                    utl::always_assert(std::holds_alternative<hir::expression::Block>(function.body.value));

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

                    space->definitions_in_order.emplace_back(child);
                    space->lower_table.add_new_or_abort(hir_child.name.identifier, child);

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
                    space->definitions_in_order.emplace_back(info);
                    context.nameless_entities.implementations.push_back(info);
                },
                [&](hir::definition::Instantiation& instantiation) {
                    utl::wrapper auto const info = utl::wrap(Instantiation_info {
                        .value          = std::move(instantiation),
                        .home_namespace = space
                    });
                    space->definitions_in_order.emplace_back(info);
                    context.nameless_entities.instantiations.push_back(info);
                },
                [&](hir::definition::Implementation_template& implementation_template) {
                    utl::wrapper auto const info = utl::wrap(Implementation_template_info {
                        .value          = std::move(implementation_template),
                        .home_namespace = space
                    });
                    space->definitions_in_order.emplace_back(info);
                    context.nameless_entities.implementation_templates.push_back(info);
                },
                [&](hir::definition::Instantiation_template& instantiation_template) {
                    utl::wrapper auto const info = utl::wrap(Instantiation_template_info {
                        .value          = std::move(instantiation_template),
                        .home_namespace = space
                    });
                    space->definitions_in_order.emplace_back(info);
                    context.nameless_entities.instantiation_templates.push_back(info);
                },

                [](hir::definition::Namespace_template&) {
                    utl::todo();
                }
            );
        }
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


auto compiler::resolve(Desugar_result&& desugar_result) -> Resolve_result {
    mir::Node_context node_context {
        utl::Wrapper_context<mir::Expression>    { desugar_result.node_context.arena_size<hir::Expression>() },
        utl::Wrapper_context<mir::Pattern>       { desugar_result.node_context.arena_size<hir::Pattern>() },
        utl::Wrapper_context<mir::Type::Variant> { desugar_result.node_context.arena_size<hir::Type>() },
        utl::Wrapper_context<mir::Mutability::Variant> {},
    };
    mir::Namespace_context namespace_context;

    Context context { std::move(desugar_result.source), std::move(desugar_result.compilation_info) };

    register_namespace(context, desugar_result.module.definitions, context.global_namespace);
    resolve_signatures(context, context.global_namespace);
    resolve_functions(context, context.global_namespace);
    context.solve_deferred_constraints();

    return Resolve_result {
        .compilation_info  = std::move(context.compilation_info),
        .source            = std::move(context.source),
        .node_context      = std::move(node_context),
        .namespace_context = std::move(namespace_context),
        .module            = std::move(context.output_module),
    };
}