# Kieli TODO

## liblex
- Single-character operator tokens, glue in the parser?

## libparse
- `struct Parse_state`
- `auto parse(Parse_state& state) -> std::expected<Definition, EOF or error>`
- Accept trailing commas
- Test definition parsing
- Error node, error recovery

## libdesugar
- Tests
- Shunting yard?

## libresolve
- Inject built in types
- Unique zero-sized type for each function, coerce to function pointer type
- Scope stack, scope id
- Generate logs for rollback
- Attribute `linear` means type has to be explicitly dropped with a function marked `drop`
- Interned HIR

## libcompiler
- Document paths should always be absolute
- Generic tree walkers
- `enum class Phase { lex, parse, etc. }`
- `enum class liblex::diagnostic`
- `auto diag_code(Phase, std::size_t code) -> std::string`

## Language server
- Semantic tokens
- Handle command line properly
- Gather code actions per line
- Split executable into communication thread and analysis thread, actor mailboxes

## Build process
- Profile compilation with `-ftime-trace`
- Try the mold linker, `-fuse-ld=mold`
- git submodules?

## Miscellaneous
- Attributes/annotations
- First class testing
- Inline concepts
- Definition source hash
- Range should store width only?
- Union types `Int|Float`
- in, inout, out, mv, fwd?
- libtommath for arbitrary precision integers?
- String interpolation
- lsp character offsets are utf8
- Configure builds with `build.kieli`
- Backends: bytecode, C, x86 asm

## Look into for potential inspiration
- Swift actors
- Go generics
