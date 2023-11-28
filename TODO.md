# Kieli TODO

## liblex
- Test token description
- Reject identifiers that are unreasonably long (100 chars?)
- Generate a token stream (begin+end?) instead of producing a vector of tokens

## libparse
- Test definition parsing
- Test rejection of invalid template parameter/argument combinations
- Use a safe `Token_iterator` type instead of `Lexical_token const*`
- AST error node

## Redesign
- libname
- libtype

## Build process
- Profile compilation with `-ftime-trace`
- Try the mold linker, `-fuse-ld=mold`

## clang-format
- Assignments incorrectly align across multiple lines
- Return type constraints should stay on the same line
- Some ternary conditionals are excessively indented

## Miscellaneous
- Parameters: in, inout, out, move, forward ?
- Fix compilation phase `#include` leakage
- libbuild
- libconfigure
- User generated compile-time messages
- Remove `noexcept` speficiation from functions with preconditions
- Use libtommath for arbitrary precision integers

## To-remove
- `utl::always_false`, when `static_assert(false)` is supported by compilers
- `utl::Flatmap`, when `std::flat_map` is supported by compilers
- `bootleg::forward_like`, when `std::forward_like` is supported by compilers

## Long-term
- Language server
