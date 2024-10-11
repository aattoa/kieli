#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

namespace kieli {
    // Concrete syntax tree compilation firewall.
    struct CST {
        // Defined in `cst.hpp`
        struct Module;
        std::unique_ptr<Module> module;

        explicit CST(Module&&);
        CST(CST&&) noexcept;
        auto operator=(CST&&) noexcept -> CST&;
        ~CST();

        [[nodiscard]] auto get() const -> Module const&;
        [[nodiscard]] auto get() -> Module&;
    };
} // namespace kieli

#define DEFINE_INDEX(name)                      \
    struct name : utl::Vector_index<name> {     \
        using Vector_index<name>::Vector_index; \
    }

// NOLINTBEGIN(bugprone-forward-declaration-namespace)

namespace kieli::cst {
    struct Arena;
    struct Definition;
    struct Expression;
    struct Pattern;
    struct Type;
    DEFINE_INDEX(Expression_id);
    DEFINE_INDEX(Pattern_id);
    DEFINE_INDEX(Type_id);
} // namespace kieli::cst

namespace kieli::ast {
    struct Arena;
    struct Expression;
    struct Type;
    struct Pattern;
    struct Definition;
    DEFINE_INDEX(Expression_id);
    DEFINE_INDEX(Pattern_id);
    DEFINE_INDEX(Type_id);
} // namespace kieli::ast

namespace kieli::hir {
    struct Mutability;
    struct Expression;
    struct Pattern;
    struct Type;
    DEFINE_INDEX(Expression_id);
    DEFINE_INDEX(Pattern_id);
    DEFINE_INDEX(Type_id);
    DEFINE_INDEX(Mutability_id);
    DEFINE_INDEX(Module_id);
    DEFINE_INDEX(Environment_id);
    DEFINE_INDEX(Function_id);
    DEFINE_INDEX(Enumeration_id);
    DEFINE_INDEX(Alias_id);
    DEFINE_INDEX(Concept_id);
    DEFINE_INDEX(Constructor_id);
    DEFINE_INDEX(Type_variable_id);
    DEFINE_INDEX(Mutability_variable_id);
    DEFINE_INDEX(Scope_id);
} // namespace kieli::hir

// NOLINTEND(bugprone-forward-declaration-namespace)

#undef DEFINE_INDEX
