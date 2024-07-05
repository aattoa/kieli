#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto duplicate_definitions_error(
        kieli::Source const&       source,
        kieli::Name_dynamic const& first,
        kieli::Name_dynamic const& second) -> cppdiag::Diagnostic
    {
        return cppdiag::Diagnostic {
            .text_sections = utl::to_vector({
                kieli::text_section(
                    source, first.range, "First defined here", cppdiag::Severity::information),
                kieli::text_section(source, second.range, "Later defined here"),
            }),
            .message       = std::format("Multiple definitions for '{}'", first.identifier),
            .severity      = cppdiag::Severity::error,
        };
    }

    auto add_definition_to_environment(
        Context&                  context,
        ast::Definition&&         definition,
        hir::Environment_id const environment_id) -> void
    {
        kieli::Source_id const   source      = definition.source;
        libresolve::Environment& environment = context.arenas.environments[environment_id];

        std::visit(
            utl::Overload {
                [&](ast::definition::Enumeration&& enumeration) {
                    auto const      name = enumeration.name;
                    hir::Type const type { context.arenas.type(hir::type::Error {}), name.range };

                    auto const info = context.arenas.enumerations.push(
                        std::move(enumeration), environment_id, source, name, type);
                    type.variant.as_mutable() = hir::type::Enumeration { enumeration.name, info };

                    environment.in_order.emplace_back(info);
                    add_to_environment(context, source, environment, name, info);
                },
                [&](ast::definition::Function&& function) {
                    auto const name = function.signature.name;
                    auto const info = context.arenas.functions.push(
                        std::move(function), environment_id, source, name);

                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
                },
                [&](ast::definition::Typeclass&& typeclass) {
                    auto const name = typeclass.name;
                    auto const info = context.arenas.typeclasses.push(
                        std::move(typeclass), environment_id, source, name);

                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
                },
                [&](ast::definition::Alias&& alias) {
                    auto const name = alias.name;
                    auto const info = context.arenas.aliases.push(
                        std::move(alias), environment_id, source, name);

                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
                },
                [&](ast::definition::Submodule&& submodule) {
                    auto const name = submodule.name;
                    auto const info = context.arenas.modules.push(
                        std::move(submodule), environment_id, source, submodule.name);

                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
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
        kieli::Source_id const                 source,
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
    kieli::Source_id const  source,
    Environment&            environment,
    kieli::Name_lower const name,
    Lower_info::Variant     variant) -> void
{
    do_add_to_environment<Lower_info>(
        context, source, environment.lower_map, name, std::move(variant));
}

auto libresolve::add_to_environment(
    Context&                context,
    kieli::Source_id const  source,
    Environment&            environment,
    kieli::Name_upper const name,
    Upper_info::Variant     variant) -> void
{
    do_add_to_environment<Upper_info>(
        context, source, environment.upper_map, name, std::move(variant));
}

auto libresolve::collect_environment(
    Context&                       context,
    kieli::Source_id const         source,
    std::vector<ast::Definition>&& definitions) -> hir::Environment_id
{
    auto const environment = context.arenas.environments.push(Environment { .source = source });
    for (ast::Definition& definition : definitions) {
        add_definition_to_environment(context, std::move(definition), environment);
    }
    return environment;
}
