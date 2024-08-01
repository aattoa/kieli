#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto duplicate_definitions_error(
        kieli::Source const& source,
        kieli::Name const&   first,
        kieli::Name const&   second) -> cppdiag::Diagnostic
    {
        return cppdiag::Diagnostic {
            .text_sections = utl::to_vector({
                kieli::text_section(
                    source, first.range, "First defined here", cppdiag::Severity::information),
                kieli::text_section(source, second.range, "Later defined here"),
            }),
            .message       = std::format("Multiple definitions for '{}'", first),
            .severity      = cppdiag::Severity::error,
        };
    }

    auto add_definition_to_environment(
        Context&                  context,
        ast::Definition&&         definition,
        hir::Environment_id const environment_id) -> void
    {
        kieli::Source_id const   source      = definition.source;
        libresolve::Environment& environment = context.info.environments[environment_id];

        std::visit(
            utl::Overload {
                [&](ast::definition::Enumeration&& enumeration) {
                    auto const name = enumeration.name;
                    auto const type = context.hir.types.push(hir::type::Error {});
                    auto const id   = context.info.enumerations.push(
                        std::move(enumeration),
                        environment_id,
                        source,
                        name,
                        hir::Type { type, name.range });
                    context.hir.types[type] = hir::type::Enumeration { enumeration.name, id };
                    environment.in_order.emplace_back(id);
                    add_to_environment(context, source, environment, name, id);
                },

                [&](ast::definition::Function&& function) {
                    auto const name = function.signature.name;
                    auto const info = context.info.functions.push(
                        std::move(function), environment_id, source, name);
                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
                },

                [&](ast::definition::Typeclass&& typeclass) {
                    auto const name = typeclass.name;
                    auto const info = context.info.typeclasses.push(
                        std::move(typeclass), environment_id, source, name);
                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
                },

                [&](ast::definition::Alias&& alias) {
                    auto const name = alias.name;
                    auto const info
                        = context.info.aliases.push(std::move(alias), environment_id, source, name);
                    add_to_environment(context, source, environment, name, info);
                    environment.in_order.emplace_back(info);
                },

                [&](ast::definition::Submodule&& submodule) {
                    auto const name = submodule.name;
                    auto const info = context.info.modules.push(
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
        Context&                 context,
        kieli::Source_id const   source,
        auto&                    map,
        auto const               name,
        typename Info::Variant&& variant) -> void
    {
        if (auto const it = std::ranges::find(map, name.identifier, utl::first); it != map.end()) {
            context.db.diagnostics.push_back(
                duplicate_definitions_error(context.db.sources[source], it->second.name, name));
            throw kieli::Compilation_failure {};
        }
        map.emplace_back(name.identifier, Info { name, source, std::move(variant) });
    }
} // namespace

auto libresolve::add_to_environment(
    Context&               context,
    kieli::Source_id const source,
    Environment&           environment,
    kieli::Lower const     name,
    Lower_info::Variant    variant) -> void
{
    do_add_to_environment<Lower_info>(
        context, source, environment.lower_map, name, std::move(variant));
}

auto libresolve::add_to_environment(
    Context&               context,
    kieli::Source_id const source,
    Environment&           environment,
    kieli::Upper const     name,
    Upper_info::Variant    variant) -> void
{
    do_add_to_environment<Upper_info>(
        context, source, environment.upper_map, name, std::move(variant));
}

auto libresolve::collect_environment(
    Context&                       context,
    kieli::Source_id const         source,
    std::vector<ast::Definition>&& definitions) -> hir::Environment_id
{
    auto const environment = context.info.environments.push(Environment { .source = source });
    for (ast::Definition& definition : definitions) {
        add_definition_to_environment(context, std::move(definition), environment);
    }
    return environment;
}
