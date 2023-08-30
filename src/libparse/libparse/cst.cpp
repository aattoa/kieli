#include <libparse/cst.hpp>
#include <libutl/common/utilities.hpp>

auto cst::Token::from_lexical(kieli::Lexical_token const* const pointer) -> Token
{
    assert(pointer != nullptr && pointer->type != kieli::Lexical_token::Type::end_of_input);
    return Token {
        .source_view      = pointer->source_view,
        .preceding_trivia = pointer->preceding_trivia,
        .trailing_trivia  = pointer[1].preceding_trivia,
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

auto cst::Template_argument::source_view() const -> utl::Source_view
{
    return utl::match(
        value,
        [](Wildcard const& wildcard) { return wildcard.source_view; },
        [](utl::Wrapper<Type> const type) { return type->source_view; },
        [](utl::Wrapper<Expression> const expression) { return expression->source_view; },
        [](cst::Mutability const& mutability) { return mutability.source_view; });
}
