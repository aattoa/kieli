# Kieli TODO

## liblex
- Integer types should be simple identifiers
- Test token description
- Reject identifiers that are unreasonably long (100 chars?)
- Delay numeric literal parsing to libparse

## libparse
- Test definition parsing
- Error node, error recovery

## libdesugar
- More tests

## Build process
- Profile compilation with `-ftime-trace`
- Try the mold linker, `-fuse-ld=mold`

## Miscellaneous
- Parameters: in, inout, out, move, forward ?
- libbuild
- libconfigure
- User generated compile-time messages
- Use libtommath for arbitrary precision integers
- Wrapper `unwrap` instead of `operator*` ?

## To-remove
- `utl::Flatmap`, when `std::flat_map` is supported by compilers

## Long-term
- Language server
