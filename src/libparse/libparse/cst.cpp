#include <libparse/cst.hpp>
#include <libutl/common/utilities.hpp>

auto cst::Token::from_lexical(kieli::Token const& lexical) -> Token
{
    return Token {
        .source_range     = lexical.source_range,
        .preceding_trivia = lexical.preceding_trivia,
    };
}

auto cst::Self_parameter::is_reference() const noexcept -> bool
{
    return ampersand_token.has_value();
}

auto cst::Qualified_name::is_upper() const noexcept -> bool
{
    return primary_name.is_upper.get();
}

auto cst::Qualified_name::is_unqualified() const noexcept -> bool
{
    return !root_qualifier.has_value() && middle_qualifiers.elements.empty();
}

auto cst::Template_parameter::kind_description(const Variant& variant) noexcept -> std::string_view
{
    return std::visit(
        utl::Overload {
            [](cst::Template_parameter::Type_parameter const&) { return "type"; },
            [](cst::Template_parameter::Value_parameter const&) { return "value"; },
            [](cst::Template_parameter::Mutability_parameter const&) { return "mutability"; },
        },
        variant);
}

auto cst::Template_argument::kind_description(const Variant& variant) noexcept -> std::string_view
{
    return std::visit(
        utl::Overload {
            [](cst::Template_argument::Wildcard const&) { return "wildcard"; },
            [](utl::Wrapper<cst::Type> const&) { return "type"; },
            [](utl::Wrapper<cst::Expression> const&) { return "value"; },
            [](cst::Mutability const&) { return "mutability"; },
        },
        variant);
}

auto cst::Template_argument::source_range() const -> utl::Source_range
{
    return std::visit(
        utl::Overload {
            [](Wildcard const& wildcard) { return wildcard.source_range; },
            [](utl::Wrapper<Type> const type) { return type->source_range; },
            [](utl::Wrapper<Expression> const expression) { return expression->source_range; },
            [](cst::Mutability const& mutability) { return mutability.source_range; },
        },
        value);
}
