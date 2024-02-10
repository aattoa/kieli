#pragma once

#include <libresolve/hir.hpp>

struct libresolve::hir::Unification_mutability_variable_state {
    struct Solved {
        Mutability solution;
    };

    struct Unsolved {
        Unification_variable_tag tag;
    };

    std::variant<Solved, Unsolved> variant;
};

struct libresolve::hir::Unification_type_variable_state {
    struct Solved {
        Type solution;
    };

    struct Unsolved {
        Unification_variable_tag       tag;
        Unification_type_variable_kind kind {};
    };

    std::variant<Solved, Unsolved> variant;
};

namespace libresolve {
    using Unification_variable_state_arena = utl::Wrapper_arena<
        hir::Unification_type_variable_state,
        hir::Unification_mutability_variable_state>;

    class Unification_state {
        Unification_variable_state_arena m_state_arena
            = Unification_variable_state_arena::with_default_page_size();
        std::size_t m_current_variable_tag {};
    public:
        auto fresh_general_type_variable()
            -> utl::Mutable_wrapper<hir::Unification_type_variable_state>;
        auto fresh_integral_type_variable()
            -> utl::Mutable_wrapper<hir::Unification_type_variable_state>;
        auto fresh_mutability_variable()
            -> utl::Mutable_wrapper<hir::Unification_mutability_variable_state>;
    private:
        auto fresh_tag() -> hir::Unification_variable_tag;
    };
} // namespace libresolve
