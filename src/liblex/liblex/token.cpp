#include <libutl/common/utilities.hpp>
#include <liblex/token.hpp>

using Token = compiler::Lexical_token;


auto Token::as_floating  () const noexcept -> decltype(Floating::value)  { return value_as<Floating>().value; }
auto Token::as_character () const noexcept -> decltype(Character::value) { return value_as<Character>().value; }
auto Token::as_boolean   () const noexcept -> decltype(Boolean::value)   { return value_as<Boolean>().value; }
auto Token::as_string    () const noexcept -> String                     { return value_as<String>(); }
auto Token::as_identifier() const noexcept -> Identifier                 { return value_as<Identifier>(); }

auto Token::as_signed_integer() const noexcept -> utl::Isize {
    if (auto const* const integer = std::get_if<Integer_of_unknown_sign>(&value))
        return integer->value;
    else
        return value_as<Signed_integer>().value;
}
auto Token::as_unsigned_integer() const noexcept -> utl::Usize {
    if (auto const* const integer = std::get_if<Integer_of_unknown_sign>(&value)) {
        assert(std::in_range<utl::Usize>(integer->value));
        return static_cast<utl::Usize>(integer->value);
    }
    else {
        return value_as<Unsigned_integer>().value;
    }
}
