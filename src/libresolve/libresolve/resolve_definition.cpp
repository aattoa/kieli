#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::resolve_function(Context& context, Function_info& function) -> hir::Function&
{
    (void)context;
    (void)function;
    cpputil::todo();
}

auto libresolve::resolve_enumeration(Context& context, Enumeration_info& enumeration)
    -> hir::Enumeration&
{
    (void)context;
    (void)enumeration;
    cpputil::todo();
}

auto libresolve::resolve_typeclass(Context& context, Typeclass_info& typeclass) -> hir::Typeclass&
{
    (void)context;
    (void)typeclass;
    cpputil::todo();
}

auto libresolve::resolve_alias(Context& context, Alias_info& alias) -> hir::Alias&
{
    (void)context;
    (void)alias;
    cpputil::todo();
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
            [&](utl::Mutable_wrapper<Function_info> const function) {
                (void)resolve_function(context, function.as_mutable());
            },
            [&](utl::Mutable_wrapper<Module_info> const module) {
                (void)resolve_module(context, module.as_mutable());
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
