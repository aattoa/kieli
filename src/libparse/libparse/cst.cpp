#include <libutl/common/utilities.hpp>
#include <libparse/cst.hpp>


auto cst::Token::from_lexical(kieli::Lexical_token const* const pointer) -> Token {
    assert(pointer != nullptr && pointer->type != kieli::Lexical_token::Type::end_of_input);
    return Token {
        .source_view      = pointer->source_view,
        .preceding_trivia = pointer->preceding_trivia,
        .trailing_trivia  = pointer[1].preceding_trivia,
    };
}

auto cst::Name_dynamic::as_upper() const noexcept -> Name_upper {
    utl::always_assert(is_upper.get());
    return { identifier, source_view };
}
auto cst::Name_dynamic::as_lower() const noexcept -> Name_lower {
    utl::always_assert(!is_upper.get());
    return { identifier, source_view };
}
cst::Name_upper::operator Name_dynamic() const noexcept {
    return { identifier, source_view, true };
}
cst::Name_lower::operator Name_dynamic() const noexcept {
    return { identifier, source_view, false };
}

auto cst::Self_parameter::is_reference() const noexcept -> bool {
    return ampersand_token.has_value();
}
auto cst::Qualified_name::is_upper() const noexcept -> bool {
    return primary_name.is_upper.get();
}
auto cst::Qualified_name::is_unqualified() const noexcept -> bool {
    return !root_qualifier.has_value() && middle_qualifiers.elements.empty();
}
