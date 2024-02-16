#pragma once

namespace libresolve {
    struct Function_info;
    struct Enumeration_info;
    struct Typeclass_info;
    struct Alias_info;
    struct Module_info;
    struct Environment;
    class Scope;
} // namespace libresolve

namespace libresolve::hir {
    struct Mutability;
    struct Expression;
    struct Pattern;
    struct Type;
    struct Unification_type_variable_state;
    struct Unification_mutability_variable_state;
} // namespace libresolve::hir
