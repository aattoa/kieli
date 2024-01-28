#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libresolve/type.hpp>

namespace libresolve::hir {

    namespace expression {}

    struct Expression {
        using Variant = std::monostate;
        Variant           variant;
        libresolve::Type  type;
        utl::Source_range source_range;
    };

    struct Structure {};

    struct Enumeration {};

    struct Typeclass {};

    struct Alias {};

    struct Namespace {};

    struct Function {};

} // namespace libresolve::hir
