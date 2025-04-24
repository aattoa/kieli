# Kieli TODO

## libparse
- Accept trailing commas
- Test definition parsing
- Integer exponents
- Digit separators

## libdesugar
- Tests

## libresolve
- Inject built in types
- Unique zero-sized type for each function, coerce to function pointer type
- Unification rollback logs?
- Attribute `linear` means type has to be explicitly dropped with a function marked `drop`
- Handle duplicate definitions

## libcompiler
- Document paths should always be absolute
- Generic tree walkers

## Language server
- Semantic tokens for escape sequences
- Handle command line properly
- Avoid collecting type hints (etc.) when the client does not support them
- Split executable into communication thread and analysis thread, actor mailboxes

## Build process
- Profile compilation with `-ftime-trace`
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
- Configure builds with `build.kieli`
- Backends: bytecode, C, x86 asm

## Look into for potential inspiration
- Swift actors
- Go generics
