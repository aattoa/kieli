#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    [[noreturn]] auto report_duplicate_definitions_error(
        kieli::Diagnostics&        diagnostics,
        utl::Source::Wrapper const source,
        kieli::Name_dynamic const& first,
        kieli::Name_dynamic const& second) -> void
    {
        diagnostics.error(
            {
                kieli::Simple_text_section {
                    .source       = source,
                    .source_range = first.source_range,
                    .note         = "First defined here",
                    .severity     = cppdiag::Severity::information,
                },
                kieli::Simple_text_section {
                    .source       = source,
                    .source_range = second.source_range,
                    .note         = "Later defined here",
                },
            },
            "Duplicate definitions of '{}' in the same module",
            first.identifier);
    }

    template <class Info>
    auto make_info(
        libresolve::Context&                  context,
        libresolve::Environment_wrapper const environment,
        auto const                            name,
        typename Info::Variant&&              variant) -> utl::Mutable_wrapper<Info>
    {
        return context.arenas.info_arena.wrap<Info, utl::Wrapper_mutability::yes>(Info {
            .variant     = std::move(variant),
            .environment = environment,
            .name        = name,
        });
    }

    template <class Info, bool is_upper>
    auto add_definition(
        libresolve::Context&                  context,
        utl::Source::Wrapper const            source,
        libresolve::Environment_wrapper const environment,
        kieli::Basic_name<is_upper> const     name,
        typename Info::Variant&&              variant) -> void
    {
        auto const info = make_info<Info>(context, environment, name, std::move(variant));
        environment.as_mutable().in_order.push_back(info);
        libresolve::add_to_environment(context, source, environment, name, info);
    }

    auto add_definition_to_environment(
        libresolve::Context&                  context,
        ast::Definition&&                     definition,
        libresolve::Environment_wrapper const environment) -> void
    {
        utl::Source::Wrapper const source = definition.source;
        std::visit(
            utl::Overload {
                [&](ast::definition::Function&& function) {
                    add_definition<libresolve::Function_info>(
                        context, source, environment, function.signature.name, std::move(function));
                },
                [&](ast::definition::Enum&& enumeration) {
                    add_definition<libresolve::Enumeration_info>(
                        context, source, environment, enumeration.name, std::move(enumeration));
                },
                [&](ast::definition::Typeclass&& typeclass) {
                    add_definition<libresolve::Typeclass_info>(
                        context, source, environment, typeclass.name, std::move(typeclass));
                },
                [&](ast::definition::Alias&& alias) {
                    add_definition<libresolve::Alias_info>(
                        context, source, environment, alias.name, std::move(alias));
                },
                [&](ast::definition::Submodule&& submodule) {
                    add_definition<libresolve::Module_info>(
                        context, source, environment, submodule.name, std::move(submodule));
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
            std::move(definition.value));
    }

    template <class Info>
    auto do_add_to_environment(
        libresolve::Context&                   context,
        utl::Source::Wrapper const             source,
        utl::Flatmap<kieli::Identifier, Info>& environment,
        auto const                             name,
        typename Info::Variant&&               variant) -> void
    {
        if (auto const* const existing = environment.find(name.identifier)) {
            report_duplicate_definitions_error(
                context.compile_info.diagnostics,
                source,
                existing->name.as_dynamic(),
                name.as_dynamic());
        }
        environment.add_new_unchecked(name.identifier, Info { name, source, std::move(variant) });
    }
} // namespace

auto libresolve::collect_environment(Context& context, std::vector<ast::Definition>&& definitions)
    -> utl::Mutable_wrapper<libresolve::Environment>
{
    auto const environment = context.arenas.environment_arena.wrap<utl::Wrapper_mutability::yes>();
    for (ast::Definition& definition : definitions) {
        add_definition_to_environment(context, std::move(definition), environment);
    }
    return environment;
}

auto libresolve::add_to_environment(
    Context&                   context,
    utl::Source::Wrapper const source,
    Environment_wrapper const  environment,
    kieli::Name_lower const    name,
    Lower_info::Variant        variant) -> void
{
    do_add_to_environment<Lower_info>(
        context, source, environment.as_mutable().lower_map, name, std::move(variant));
}

auto libresolve::add_to_environment(
    Context&                   context,
    utl::Source::Wrapper const source,
    Environment_wrapper const  environment,
    kieli::Name_upper const    name,
    Upper_info::Variant        variant) -> void
{
    do_add_to_environment<Upper_info>(
        context, source, environment.as_mutable().upper_map, name, std::move(variant));
}
