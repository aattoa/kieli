#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto duplicate_definitions_error(
        kieli::Document_id const document_id,
        kieli::Identifier const  identifier,
        kieli::Range const&      first,
        kieli::Range const&      second) -> kieli::Diagnostic
    {
        return kieli::Diagnostic {
            .message      = std::format("Multiple definitions for '{}'", identifier),
            .range        = second,
            .severity     = kieli::Severity::error,
            .related_info = utl::to_vector({
                kieli::Diagnostic_related_info {
                    .message = "First defined here",
                    .location { .document_id = document_id, .range = first },
                },
            }),
        };
    }

    auto add_definition_to_environment(
        Context&                  context,
        ast::Definition&&         definition,
        hir::Environment_id const environment_id) -> void
    {
        kieli::Document_id const source      = definition.document_id;
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

                [&](ast::definition::Concept&& concept_) {
                    auto const name = concept_.name;
                    auto const info = context.info.concepts.push(
                        std::move(concept_), environment_id, source, name);
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
            },
            std::move(definition.variant));
    }

    template <class Info>
    auto do_add_to_environment(
        Context&                 context,
        kieli::Document_id const document_id,
        auto&                    map,
        auto const               name,
        typename Info::Variant&& variant) -> void
    {
        if (auto const it = std::ranges::find(map, name.identifier, utl::first); it != map.end()) {
            kieli::Diagnostic diagnostic = duplicate_definitions_error(
                document_id, name.identifier, it->second.name.range, name.range);
            kieli::fatal_error(context.db, document_id, std::move(diagnostic));
        }
        map.emplace_back(name.identifier, Info { name, document_id, std::move(variant) });
    }
} // namespace

auto libresolve::add_to_environment(
    Context&                 context,
    kieli::Document_id const document_id,
    Environment&             environment,
    kieli::Lower const       name,
    Lower_info::Variant      variant) -> void
{
    do_add_to_environment<Lower_info>(
        context, document_id, environment.lower_map, name, std::move(variant));
}

auto libresolve::add_to_environment(
    Context&                 context,
    kieli::Document_id const document_id,
    Environment&             environment,
    kieli::Upper const       name,
    Upper_info::Variant      variant) -> void
{
    do_add_to_environment<Upper_info>(
        context, document_id, environment.upper_map, name, std::move(variant));
}

auto libresolve::collect_environment(
    Context&                       context,
    kieli::Document_id const       document_id,
    std::vector<ast::Definition>&& definitions) -> hir::Environment_id
{
    auto const environment_id
        = context.info.environments.push(Environment { .document_id = document_id });
    for (ast::Definition& definition : definitions) {
        add_definition_to_environment(context, std::move(definition), environment_id);
    }
    return environment_id;
}
