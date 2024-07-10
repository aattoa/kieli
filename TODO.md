# Kieli TODO

## liblex
- Integer types should be simple identifiers
- Delay numeric literal parsing to libparse?

## libparse
- Test definition parsing
- Error node, error recovery

## libdesugar
- More tests

## libresolve
- Unique zero-sized type for each function, coerce to function pointer type
- Scope stack, scope id
- Generate logs for rollback

## Build process
- Profile compilation with `-ftime-trace`
- Try the mold linker, `-fuse-ld=mold`

## Miscellaneous
- Range should store width only?
- Union types `Int|Float`
- Allow omission of repeated parameter types ?
- in, inout, out, mv, fwd ?
- libtommath for arbitrary precision integers?
- String interpolation
- First class testing
- lsp column offsets are utf8
- Configure builds with `build.kieli`
- Backends: bytecode, C, x86 asm

## To-remove
- `utl::Flatmap`, when `std::flat_map` is supported by compilers
- `utl::Wrapper`, use index vectors instead
