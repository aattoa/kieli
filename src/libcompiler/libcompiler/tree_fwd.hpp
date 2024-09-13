#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

// This file provides compilation firewalls for syntax trees, which speeds up compilation.
// Source files that must access the tree structures can include their definition headers.

namespace kieli {

    // Concrete syntax tree
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

namespace kieli::hir {

    enum class Type_variable_kind { general, integral };

    enum class Expression_kind { place, value };

    struct Expression_id : utl::Vector_index<Expression_id> {
        using Vector_index::Vector_index;
    };

    struct Pattern_id : utl::Vector_index<Pattern_id> {
        using Vector_index::Vector_index;
    };

    struct Type_id : utl::Vector_index<Type_id> {
        using Vector_index::Vector_index;
    };

    struct Mutability_id : utl::Vector_index<Mutability_id> {
        using Vector_index::Vector_index;
    };

    struct Module_id : utl::Vector_index<Module_id> {
        using Vector_index::Vector_index;
    };

    struct Environment_id : utl::Vector_index<Environment_id> {
        using Vector_index::Vector_index;
    };

    struct Function_id : utl::Vector_index<Function_id> {
        using Vector_index::Vector_index;
    };

    struct Enumeration_id : utl::Vector_index<Enumeration_id> {
        using Vector_index::Vector_index;
    };

    struct Alias_id : utl::Vector_index<Alias_id> {
        using Vector_index::Vector_index;
    };

    struct Concept_id : utl::Vector_index<Concept_id> {
        using Vector_index::Vector_index;
    };

    struct Constructor_id : utl::Vector_index<Constructor_id> {
        using Vector_index::Vector_index;
    };

    struct Type_variable_id : utl::Vector_index<Type_variable_id> {
        using Vector_index::Vector_index;
    };

    struct Mutability_variable_id : utl::Vector_index<Mutability_variable_id> {
        using Vector_index::Vector_index;
    };

} // namespace kieli::hir
