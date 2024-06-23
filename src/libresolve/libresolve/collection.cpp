#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    auto duplicate_definitions_error(
        utl::Source const&         source,
        kieli::Name_dynamic const& first,
        kieli::Name_dynamic const& second) -> cppdiag::Diagnostic
    {
        return cppdiag::Diagnostic {
            .text_sections = utl::to_vector({
                kieli::text_section(
                    source,
                    first.source_range,
                    "First defined here",
                    cppdiag::Severity::information),
                kieli::text_section(source, second.source_range, "Later defined here"),
            }),
            .message       = std::format("Multiple definitions for '{}'", first.identifier),
            .severity      = cppdiag::Severity::error,
        };
    }

    auto add_definition_to_environment(
        Context&                  context,
        ast::Definition&&         definition,
        Environment_wrapper const environment) -> void
    {
        utl::Source_id const source = definition.source;
        std::visit(
            utl::Overload {
                [&](ast::definition::Enumeration&& enumeration) {
                    auto const info = context.arenas.info(Enumeration_info {
                        .variant     = std::move(enumeration),
                        .environment = environment,
                        .name        = enumeration.name,
                        .type {
                            context.arenas.type(hir::type::Error {}), // Overwritten below
                            enumeration.name.source_range,
                        },
                    });
                    environment.as_mutable().in_order.emplace_back(info);
                    add_to_environment(context, source, environment.as_mutable(), info->name, info);
                    info->type.variant.as_mutable()
                        = hir::type::Enumeration { enumeration.name, info };
                },
                [&](ast::definition::Function&& function) {
                    auto const info = context.arenas.info(Function_info {
                        .variant     = std::move(function),
                        .environment = environment,
                        .name        = function.signature.name,
                    });
                    add_to_environment(context, source, environment.as_mutable(), info->name, info);
                    environment.as_mutable().in_order.emplace_back(info);
                },
                [&](ast::definition::Typeclass&& typeclass) {
                    auto const info = context.arenas.info(Typeclass_info {
                        .variant     = std::move(typeclass),
                        .environment = environment,
                        .name        = typeclass.name,
                    });
                    add_to_environment(context, source, environment.as_mutable(), info->name, info);
                    environment.as_mutable().in_order.emplace_back(info);
                },
                [&](ast::definition::Alias&& alias) {
                    auto const info = context.arenas.info(Alias_info {
                        .variant     = std::move(alias),
                        .environment = environment,
                        .name        = alias.name,
                    });
                    add_to_environment(context, source, environment.as_mutable(), info->name, info);
                    environment.as_mutable().in_order.emplace_back(info);
                },
                [&](ast::definition::Submodule&& submodule) {
                    auto const info = context.arenas.info(Module_info {
                        .variant     = std::move(submodule),
                        .environment = environment,
                        .name        = submodule.name,
                    });
                    add_to_environment(context, source, environment.as_mutable(), info->name, info);
                    environment.as_mutable().in_order.emplace_back(info);
                },
                [&](ast::definition::Implementation&& implementation) {
                    (void)implementation;
                    cpputil::todo();
                },
                [&](ast::definition::Instantiation&& instantiation) {
                    (void)instantiation;
                    cpputil::todo();
                },
            },
            std::move(definition.variant));
    }

    template <class Info>
    auto do_add_to_environment(
        Context&                               context,
        utl::Source_id const                   source,
        utl::Flatmap<kieli::Identifier, Info>& map,
        auto const                             name,
        typename Info::Variant&&               variant) -> void
    {
        if (auto const* const existing = map.find(name.identifier)) {
            context.compile_info.diagnostics.push_back(duplicate_definitions_error(
                context.compile_info.source_vector[source],
                existing->name.as_dynamic(),
                name.as_dynamic()));
            throw kieli::Compile_info {};
        }
        map.add_new_unchecked(name.identifier, Info { name, source, std::move(variant) });
    }
} // namespace

auto libresolve::add_to_environment(
    Context&                context,
    utl::Source_id const    source,
    Environment&            environment,
    kieli::Name_lower const name,
    Lower_info::Variant     variant) -> void
{
    do_add_to_environment<Lower_info>(
        context, source, environment.lower_map, name, std::move(variant));
}

auto libresolve::add_to_environment(
    Context&                context,
    utl::Source_id const    source,
    Environment&            environment,
    kieli::Name_upper const name,
    Upper_info::Variant     variant) -> void
{
    do_add_to_environment<Upper_info>(
        context, source, environment.upper_map, name, std::move(variant));
}

auto libresolve::collect_environment(
    Context&                       context,
    utl::Source_id const           source,
    std::vector<ast::Definition>&& definitions) -> Environment_wrapper
{
    auto const environment
        = context.arenas.environment_arena.wrap_mutable(Environment { .source = source });
    for (ast::Definition& definition : definitions) {
        add_definition_to_environment(context, std::move(definition), environment);
    }
    return environment;
}
