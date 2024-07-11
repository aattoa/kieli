#pragma once

#include <libutl/utilities.hpp>

// This file provides compilation firewalls for syntax trees, which speeds up compilation.
// Source files that must access the tree structures can include their definition headers.

namespace kieli {

    // Concrete syntax tree
    struct CST {
        // Defined in `cst.hpp`
        struct Module;
        std::unique_ptr<Module> module;

        explicit CST(Module&&);
        CST(CST&&) = default;
        ~CST();
    };

    // Abstract syntax tree
    struct AST {
        // Defined in `ast.hpp`
        struct Module;
        std::unique_ptr<Module> module;

        explicit AST(Module&&);
        AST(AST&&) = default;
        ~AST();
    };

} // namespace kieli
