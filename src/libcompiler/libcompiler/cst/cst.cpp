#include <libutl/utilities.hpp>
#include <libcompiler/cst/cst.hpp>

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
