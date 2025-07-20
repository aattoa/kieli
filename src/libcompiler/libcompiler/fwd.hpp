#ifndef KIELI_LIBCOMPILER_FWD
#define KIELI_LIBCOMPILER_FWD

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

#define DEFINE_INDEX(name)                                 \
    struct name : utl::Vector_index<name, std::uint32_t> { \
        using Vector_index::Vector_index;                  \
    }

// NOLINTBEGIN(bugprone-forward-declaration-namespace)

namespace ki::db {
    DEFINE_INDEX(Symbol_id);
    DEFINE_INDEX(Document_id);
    DEFINE_INDEX(Environment_id);
} // namespace ki::db

namespace ki::cst {
    struct Arena;
    struct Expression;
    struct Pattern;
    struct Type;
    DEFINE_INDEX(Expression_id);
    DEFINE_INDEX(Pattern_id);
    DEFINE_INDEX(Type_id);
} // namespace ki::cst

namespace ki::ast {
    struct Arena;
    struct Expression;
    struct Type;
    struct Pattern;
    struct Definition;
    DEFINE_INDEX(Expression_id);
    DEFINE_INDEX(Pattern_id);
    DEFINE_INDEX(Type_id);
} // namespace ki::ast

namespace ki::hir {
    struct Mutability;
    struct Expression;
    struct Pattern;
    struct Type;
    DEFINE_INDEX(Expression_id);
    DEFINE_INDEX(Pattern_id);
    DEFINE_INDEX(Type_id);
    DEFINE_INDEX(Mutability_id);
    DEFINE_INDEX(Module_id);
    DEFINE_INDEX(Function_id);
    DEFINE_INDEX(Structure_id);
    DEFINE_INDEX(Enumeration_id);
    DEFINE_INDEX(Constructor_id);
    DEFINE_INDEX(Field_id);
    DEFINE_INDEX(Alias_id);
    DEFINE_INDEX(Concept_id);
    DEFINE_INDEX(Type_variable_id);
    DEFINE_INDEX(Mutability_variable_id);
    DEFINE_INDEX(Local_variable_id);
    DEFINE_INDEX(Local_mutability_id);
    DEFINE_INDEX(Local_type_id);
} // namespace ki::hir

// NOLINTEND(bugprone-forward-declaration-namespace)

#undef DEFINE_INDEX

#endif // KIELI_LIBCOMPILER_FWD
