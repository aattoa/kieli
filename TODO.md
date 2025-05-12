# Kieli TODO

## libparse
- Accept trailing commas
- Use matching parentheses for recovery

## libdesugar
- Handle integer literals prefix/suffix

## libresolve
- Unique zero-sized type for each function, coerce to function pointer type
- Unification rollback logs?
- Warn when last expression in block is defer

## libcompiler
- Document paths should always be absolute
- Generic tree walkers

## Language server
- Semantic tokens for escape sequences
- Handle command line properly
- Avoid collecting type hints (etc.) when the client does not support them
- Option to display intermediate representations on hover
- Thread pool, monitor client PID on main thread

## Miscellaneous
- Attributes/annotations
- First class testing
- Inline concepts
- Range should store width only?
- Union types `Int|Float`
- libtommath for arbitrary precision integers?
- String interpolation
- Configure builds with `build.kieli`
- Backends: bytecode, C, x86 asm
