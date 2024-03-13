#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::resolve_function(Context& context, Function_info& info) -> hir::Function&
{
    if (auto* const function = std::get_if<ast::definition::Function>(&info.variant)) {
        (void)context;
        cpputil::todo();
    }
    if (auto* const function = std::get_if<Function_with_resolved_signature>(&info.variant)) {
        (void)context;
        cpputil::todo();
    }
    return std::get<hir::Function>(info.variant);
}

auto libresolve::resolve_function_signature(Context& context, Function_info& info)
    -> hir::Function_signature&
{
    if (auto* const function = std::get_if<ast::definition::Function>(&info.variant)) {
        (void)context;
        cpputil::todo();
    }
    if (auto* const function = std::get_if<Function_with_resolved_signature>(&info.variant)) {
        return function->signature;
    }
    return std::get<hir::Function>(info.variant).signature;
}

auto libresolve::resolve_enumeration(Context& context, Enumeration_info& info) -> hir::Enumeration&
{
    if (auto* const enumeration = std::get_if<ast::definition::Enumeration>(&info.variant)) {
        (void)context;
        cpputil::todo();
    }
    return std::get<hir::Enumeration>(info.variant);
}

auto libresolve::resolve_typeclass(Context& context, Typeclass_info& info) -> hir::Typeclass&
{
    if (auto* const typeclass = std::get_if<ast::definition::Typeclass>(&info.variant)) {
        (void)context;
        cpputil::todo();
    }
    return std::get<hir::Typeclass>(info.variant);
}

auto libresolve::resolve_alias(Context& context, Alias_info& info) -> hir::Alias&
{
    if (auto* const alias = std::get_if<ast::definition::Alias>(&info.variant)) {
        (void)context;
        cpputil::todo();
    }
    return std::get<hir::Alias>(info.variant);
}

auto libresolve::resolve_definitions_in_order(
    Context& context, Environment_wrapper const environment) -> void
{
    for (Definition_variant const& variant : environment->in_order) {
        resolve_definition(context, variant);
    }
}

auto libresolve::resolve_definition(Context& context, Definition_variant const& definition) -> void
{
    std::visit(
        utl::Overload {
            [&](utl::Mutable_wrapper<Module_info> const module) {
                resolve_definitions_in_order(context, resolve_module(context, module.as_mutable()));
            },
            [&](utl::Mutable_wrapper<Function_info> const function) {
                (void)resolve_function(context, function.as_mutable());
            },
            [&](utl::Mutable_wrapper<Enumeration_info> const enumeration) {
                (void)resolve_enumeration(context, enumeration.as_mutable());
            },
            [&](utl::Mutable_wrapper<Typeclass_info> const typeclass) {
                (void)resolve_typeclass(context, typeclass.as_mutable());
            },
            [&](utl::Mutable_wrapper<Alias_info> const alias) {
                (void)resolve_alias(context, alias.as_mutable());
            },
        },
        definition);
}
