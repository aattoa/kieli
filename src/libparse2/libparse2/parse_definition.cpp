#include <libutl/common/utilities.hpp>
#include <libparse2/parser_internals.hpp>

namespace {

    auto parse_function(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto parse_structure(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto parse_enumeration(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto parse_typeclass(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto parse_instantiation(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto parse_implementation(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto parse_namespace(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        (void)context;
        (void)token;
        cpputil::todo();
    }

    auto dispatch_parse_definition(libparse2::Context& context, kieli::Token2 const& token)
        -> std::optional<cst::Definition::Variant>
    {
        // clang-format off
        switch (token.type) {
        case kieli::Token2::Type::fn:         return parse_function(context, token);
        case kieli::Token2::Type::struct_:    return parse_structure(context, token);
        case kieli::Token2::Type::enum_:      return parse_enumeration(context, token);
        case kieli::Token2::Type::class_:     return parse_typeclass(context, token);
        case kieli::Token2::Type::inst:       return parse_instantiation(context, token);
        case kieli::Token2::Type::impl:       return parse_implementation(context, token);
        case kieli::Token2::Type::namespace_: return parse_namespace(context, token);
        default:
            context.restore(token);
            return std::nullopt;
        }
        // clang-format on
    }

} // namespace

auto libparse2::parse_definition(Context& context) -> std::optional<cst::Definition>
{
    kieli::Token2 const first_token = context.extract();
    return dispatch_parse_definition(context, first_token)
        .transform([&](cst::Definition::Variant&& variant) {
            return cst::Definition {
                .value       = std::move(variant),
                .source_view = context.up_to_current(first_token.source_view),
            };
        });
}
