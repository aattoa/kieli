# Kieli TODO

## liblex
- Test token description

## libparse
- Test definition parsing
- Test rejection of invalid template parameter/argument combinations
- Use a safe `Token_iterator` type instead of `Lexical_token const*`

## libdesugar
- Emit warnings on constant conditionals
- Emit errors on:
    - member redefinition
    - constructor redefinition
    - duplicate member initializer

## libresolve
- Fix integral type unification variable unifying with regular implicit template parameter
- The resolution phase should only get an immutable view of the AST
- Improve scope performance
- If explicit function return type is generalizable, then:
    - immediately resolve the function body
    - unify the type of the body with the return type
    - only then generalize the return type

## Redesign
- libutl-cli &rarr; libcli
- libutl-diagnostics &rarr; libdiag
- Use a separate name resolution phase before semantic analysis to enable last use analysis etc.?

## Build process
- Profile compilation with `-ftime-trace`
- Try the mold linker, `-fuse-ld=mold`

## Miscellaneous
- Parameters: in, inout, out, move, forward ?
- clang-format
- Fix compilation phase `#include` leakage
