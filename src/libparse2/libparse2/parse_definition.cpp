#include <libutl/common/utilities.hpp>
#include <libparse2/parser_internals.hpp>

namespace {

    auto parse_function(libparse2::Context& context) -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto parse_struct(libparse2::Context& context) -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto parse_enumeration(libparse2::Context& context) -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto parse_class(libparse2::Context& context) -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto parse_instantiation(libparse2::Context& context) -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto parse_implementation(libparse2::Context& context)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto parse_namespace(libparse2::Context& context) -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        cpputil::todo();
    }

    auto dispatch_parse_definition(libparse2::Context& context, kieli::Token2::Type const type)
        -> std::optional<cst::Definition::Variant>
    {
        switch (type) {
        case kieli::Token2::Type::fn:
            return parse_function(context);
        case kieli::Token2::Type::struct_:
            return parse_struct(context);
        case kieli::Token2::Type::enum_:
            return parse_enumeration(context);
        case kieli::Token2::Type::class_:
            return parse_class(context);
        case kieli::Token2::Type::inst:
            return parse_instantiation(context);
        case kieli::Token2::Type::impl:
            return parse_implementation(context);
        case kieli::Token2::Type::namespace_:
            return parse_namespace(context);
        default:
            return std::nullopt;
        }
    }

} // namespace

auto libparse2::parse_definition(Context& context) -> std::optional<cst::Definition>
{
    kieli::Token2 const first_token = context.peek();
    return dispatch_parse_definition(context, first_token.type)
        .transform([&](cst::Definition::Variant&& variant) {
            return cst::Definition {
                .value       = std::move(variant),
                .source_view = context.up_to_current(first_token.source_view),
            };
        });
}
