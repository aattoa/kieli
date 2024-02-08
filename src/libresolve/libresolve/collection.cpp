#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {

    template <bool is_upper>
    auto environment_map(libresolve::Environment& environment) -> auto&
    {
        if constexpr (is_upper) {
            return environment.upper_map;
        }
        else {
            return environment.lower_map;
        }
    }

    template <class Info, bool is_upper>
    auto add_to_environment(
        libresolve::Context&                                context,
        utl::Source::Wrapper const                          source,
        utl::Mutable_wrapper<libresolve::Environment> const environment,
        kieli::Basic_name<is_upper> const                   name,
        decltype(Info::variant)&&                           variant) -> void
    {
        auto& map = environment_map<is_upper>(environment.as_mutable());
        if (auto const* const definition = map.find(name.identifier)) {
            libresolve::report_duplicate_definitions_error(
                context.compile_info.diagnostics,
                source,
                definition->name.as_dynamic(),
                name.as_dynamic());
        }
        map.add_new_unchecked(
            name.identifier,
            std::conditional_t<is_upper, libresolve::Upper_info, libresolve::Lower_info> {
                name,
                source,
                context.arenas.info_arena.wrap<Info, utl::Wrapper_mutability::yes>(Info {
                    .variant     = std::move(variant),
                    .environment = environment,
                    .name        = name,
                }),
            });
    }

    auto add_definition_to_environment(
        libresolve::Context&                                context,
        ast::Definition&&                                   definition,
        utl::Mutable_wrapper<libresolve::Environment> const environment) -> void
    {
        utl::Source::Wrapper const source = definition.source;
        std::visit(
            utl::Overload {
                [&](ast::definition::Function&& function) {
                    add_to_environment<libresolve::Function_info>(
                        context, source, environment, function.signature.name, std::move(function));
                },
                [&](ast::definition::Enum&& enumeration) {
                    add_to_environment<libresolve::Enumeration_info>(
                        context, source, environment, enumeration.name, std::move(enumeration));
                },
                [&](ast::definition::Typeclass&& typeclass) {
                    add_to_environment<libresolve::Typeclass_info>(
                        context, source, environment, typeclass.name, std::move(typeclass));
                },
                [&](ast::definition::Alias&& alias) {
                    add_to_environment<libresolve::Alias_info>(
                        context, source, environment, alias.name, std::move(alias));
                },
                [&](ast::definition::Submodule&& submodule) {
                    add_to_environment<libresolve::Module_info>(
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

auto libresolve::report_duplicate_definitions_error(
    kieli::Diagnostics&        diagnostics,
    utl::Source::Wrapper const source,
    kieli::Name_dynamic const  first,
    kieli::Name_dynamic const  second) -> void
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
