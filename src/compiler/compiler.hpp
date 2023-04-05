#pragma once

#include "utl/diagnostics.hpp"
#include "utl/pooled_string.hpp"


namespace compiler {

    using String     = utl::Pooled_string<struct _string_tag>;
    using Identifier = utl::Pooled_string<struct _identifier_tag>;

    struct [[nodiscard]] Compilation_info {
        utl::diagnostics::Builder diagnostics;
        String::Pool              string_literal_pool;
        Identifier::Pool          identifier_pool;
    };

}