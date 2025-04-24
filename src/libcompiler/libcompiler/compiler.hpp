#ifndef KIELI_LIBCOMPILER_COMPILER
#define KIELI_LIBCOMPILER_COMPILER

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/string_pool.hpp>
#include <libcompiler/lsp.hpp>

namespace ki::db {

    // Identifier with a range.
    struct Name {
        utl::String_id id;
        lsp::Range     range;
    };

    struct Upper : Name {};

    struct Lower : Name {};

    struct Error {
        auto operator==(Error const&) const -> bool = default;
    };

    struct Integer {
        std::int64_t value {};
    };

    struct Floating {
        double value {};
    };

    struct Boolean {
        bool value {};
    };

    struct String {
        utl::String_id id;
    };

    // Represents concrete mutability.
    enum struct Mutability : std::uint8_t { Mut, Immut };

    // Returns `mut` or `immut`.
    auto mutability_string(Mutability mutability) -> std::string_view;

    // Check whether `name` starts with a capital letter, possibly preceded by underscores.
    auto is_uppercase(std::string_view name) -> bool;

} // namespace ki::db

#endif // KIELI_LIBCOMPILER_COMPILER
