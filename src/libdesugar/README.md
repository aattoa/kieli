# Kieli - libdesugar

libdesugar expands CST nodes into AST nodes by eliminating syntactic sugar.

The transformations are purely syntactic; no semantic analysis is performed.

The purpose of desugaring is to simplify semantic analysis by reducing the number of syntax constructs.
It also trivializes the implementation of future language features, as long as they can be expressed in terms of existing syntax.

Some errors are also caught at this stage, such as duplicate struct fields.

# List of transformations

## Structure to single-constructor enumeration

    struct Tag → enum Tag = Tag
    struct Point { x: I32, y: I32 } → enum Point = Point { x: I32, y: I32 }

## While loop to plain loop

    while a { b } → loop { if a { b } else { break () } }

## While-let loop to plain loop

    while let a = b { c } → loop { match b { a -> c; _ -> break () } }

## If-let to match

    if let a = b { c } else { d } → match b { a -> c; _ -> d }

## Discard to let block

    discard x → { let _: _ = x; () }

## Operator precedence resolution

    a + b * c → (a + (b * c))

## Implicit immutability

    let x: I32 = 0 → let immut x: I32 = 0
    let y: &I32 = &x → let immut y: &immut I32 = &immut x

## Implicit wildcard let type annotation

    let x = 0 → let x: _ = 0

## Implicit unit else

    if x { y } → if x { y } else { () }

## Implicit unit block result

    { x; } → { x; () }

## Implicit unit break

    break → break ()

## Implicit unit return

    ret → ret ()

## Implicit unit return type

    fn f() {} → fn f(): () {}

## Function body normalization

    fn f(): I32 = 10 → fn f(): I32 { 10 }