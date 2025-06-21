# Kieli TODO

## libparse
- Accept trailing commas
- Use matching parentheses for recovery
- Solve template type/value argument conflict

## libdesugar
- Pipe operator, replace wildcards with argument

## libresolve
- Unique zero-sized type for each function, coerce to function pointer type
- Unification rollback logs?
- Warn when last expression in block is defer
- Lazily parse module when collecting, no Definition_variant
- Do not store CST or AST when not necessary
- Move diagnostics from libdesugar
- Collect semantic tokens for identifiers on path resolution
- Provide a best-effort type when expression resolution fails

## Language server
- Semantic tokens for escape sequences
- Handle command line properly
- Avoid collecting hints, actions, and references when the client does not support them
- Option to display intermediate representations on hover
- Thread pool, monitor client PID on main thread
- Check client capability `completionList.itemDefaults`
- Snippet completions
- Code completion tests

## Attributes
- `@linear`: Apply to type definition, require bindings to be explicitly dropped
- `@inline`: Apply to function definition or call
- `@unsafe`: Apply to function definition or call
- `@test`: Apply to function definition, run on `kieli test`
- `@hide`: Apply to struct field
- `@repr`: Apply to type definition ("kieli", "C")
- `@extern`: Apply to function definition, specify calling convention
- `@untagged`: Apply to enum definition, disallow match, access with `@unsafe let`
- `@infix`: Apply to operator definition, specify precedence and associativity

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
- `kieli run`: Build and run given file or project main executable
- `kieli dump-ast`: Dump the AST for given file

## Miscellaneous
- Inline concepts
- libtommath for arbitrary precision integers?
- String interpolation
- Configure builds with `build.kieli`
- Tree-sitter grammar
- `perf record`
- Clean up HIR formatting
