#include <libutl/common/utilities.hpp>
#include <liblex/token.hpp>


auto compiler::Lexical_token::as_floating  () const noexcept -> decltype(Floating::value)  { return value_as<Floating>().value; }
auto compiler::Lexical_token::as_character () const noexcept -> decltype(Character::value) { return value_as<Character>().value; }
auto compiler::Lexical_token::as_boolean   () const noexcept -> decltype(Boolean::value)   { return value_as<Boolean>().value; }
auto compiler::Lexical_token::as_string    () const noexcept -> String                     { return value_as<String>(); }
auto compiler::Lexical_token::as_identifier() const noexcept -> Identifier                 { return value_as<Identifier>(); }

auto compiler::Lexical_token::as_signed_integer() const noexcept -> utl::Isize {
    if (auto const* const integer = std::get_if<Integer_of_unknown_sign>(&value))
        return integer->value;
    else
        return value_as<Signed_integer>().value;
}
auto compiler::Lexical_token::as_unsigned_integer() const noexcept -> utl::Usize {
    if (auto const* const integer = std::get_if<Integer_of_unknown_sign>(&value)) {
        assert(std::in_range<utl::Usize>(integer->value));
        return static_cast<utl::Usize>(integer->value);
    }
    else {
        return value_as<Unsigned_integer>().value;
    }
}

static_assert(std::is_trivially_copyable_v<compiler::Lexical_token>);
