# Kieli TODO

## liblex
- Integer types should be simple identifiers
- Test token description
- Reject identifiers that are unreasonably long (100 chars?)
- Generate a token stream (begin+end?) instead of producing a vector of tokens
- Reuse token buffer, `lex(string) -> Token`

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
- Use libtommath for arbitrary precision integers
- Wrapper `unwrap` instead of `operator*` ?

## To-remove
- `utl::Flatmap`, when `std::flat_map` is supported by compilers

## Long-term
- Language server
