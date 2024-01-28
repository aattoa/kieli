#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libphase/phase.hpp>

namespace libresolve {

    struct Type {
        struct Variant;
        utl::Mutable_wrapper<Variant> variant;
        utl::Source_range             source_range;
    };

    struct Type_constraint {
        // TODO
    };

    struct Unification_variable_tag {
        std::size_t value;

        explicit constexpr Unification_variable_tag(std::size_t const value) noexcept
            : value { value }
        {}
    };

    struct Unification_type_variable_state {
        struct Solved {
            Type type;
        };

        struct Unsolved {
            Unification_variable_tag     tag;
            std::vector<Type_constraint> constraints;
        };

        std::variant<Solved, Unsolved> value;
    };

    namespace type {
        struct Unification_variable {
            utl::Mutable_wrapper<Unification_type_variable_state> state;
        };
    } // namespace type

} // namespace libresolve

struct libresolve::Type::Variant
    : std::variant<
          kieli::built_in_type::Integer,
          kieli::built_in_type::Floating,
          kieli::built_in_type::Character,
          kieli::built_in_type::Boolean,
          kieli::built_in_type::String,
          type::Unification_variable> {
    using variant::variant;
    using variant::operator=;
};
