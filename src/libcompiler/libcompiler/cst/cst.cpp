#include <libutl/utilities.hpp>
#include <libcompiler/cst/cst.hpp>

auto cst::Token::from_lexical(kieli::Token const& lexical) -> Token
{
    return Token {
        .range            = lexical.range,
        .preceding_trivia = lexical.preceding_trivia,
    };
}

auto cst::Self_parameter::is_reference() const noexcept -> bool
{
    return ampersand_token.has_value();
}

auto cst::Path::is_simple_name() const noexcept -> bool
{
    return !root.has_value() && segments.elements.empty();
}
