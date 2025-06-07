# Kieli TODO

## libparse
- Accept trailing commas
- Use matching parentheses for recovery

## libresolve
- Unique zero-sized type for each function, coerce to function pointer type
- Unification rollback logs?
- Warn when last expression in block is defer
- Lazily parse module when collecting, no Definition_variant
- Move diagnostics from libdesugar
- Collect semantic tokens for identifiers on path resolution
- Provide a best-effort type when expression resolution fails

## Language server
- Semantic tokens for escape sequences
- Handle command line properly
- Avoid collecting hints, actions, and references when the client does not support them
- Option to display intermediate representations on hover
- Thread pool, monitor client PID on main thread
- Store edit position
    - On path resolution, if contained within segment range, set completion environment
    - On function call resolution, if contained within argument range, set signature help

## Attributes
- `@linear`: Apply to type definition, require bindings to be explicitly dropped
- `@inline`: Apply to function definition or call
- `@unsafe`: Apply to function definition or call
- `@test`: Apply to function definition, run on `kieli test`
- `@hide`: Apply to struct field
- `@repr`: Apply to type definition ("kieli", "C")
- `@extern`: Apply to function definition, specify calling convention
- `@untagged`: Apply to enum definition, disallow match, require `@unsafe` for access

## Builtins
- `@here`
- `@todo`
- `@unreachable`

## Executable
- `kieli build`: Build given file or entire project
- `kieli check`: Analyze given file or entire project but do not generate code
- `kieli clean`: Clean up compiler output directory
- `kieli format`: Format given file or entire project
- `kieli test`: Run all tests in the current project
- `kieli init`: Initialize a project in the current directory
- `kieli new`: Initialize a project in a new directory
- `kieli run`: Run given file or project main executable
- `kieli dump-ast`: Dump the AST for given file

## Miscellaneous
- Inline concepts
- Range should store width only?
- libtommath for arbitrary precision integers?
- String interpolation
- Configure builds with `build.kieli`
- Config instead of manifest
- `fn drop[T](x: T) = @builtin`
- Tree-sitter grammar
