# Kieli TODO

## liblex
- Integer types should be simple identifiers
- Delay numeric literal parsing to libparse?

## libparse
- `struct Parse_state`
- `auto parse(Parse_state& state) -> std::expected<Definition, EOF or error>`
- Accept trailing commas
- Test definition parsing
- Error node, error recovery

## libdesugar
- More tests
- Interface should expose (cst_def -> ast_def)

## libresolve
- Unique zero-sized type for each function, coerce to function pointer type
- Scope stack, scope id
- Generate logs for rollback

## Build process
- Profile compilation with `-ftime-trace`
- Try the mold linker, `-fuse-ld=mold`

## Miscellaneous
- Attributes/annotations
- Inline concepts
- git submodules
- Definition source hash
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

## Look into for potential inspiration
- Swift actors
- Go generics

## To-remove
- `utl::Wrapper`, use index vectors instead
