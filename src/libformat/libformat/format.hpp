#pragma once

#include <libutl/common/utilities.hpp>
#include <libparse/cst.hpp>


namespace kieli {
    struct Format_configuration {
        enum class Function_body {
            leave_as_is, normalize_to_block, normalize_to_equals_sign
        };
        utl::Usize    block_indentation = 4;
        Function_body function_body     = Function_body::leave_as_is;
    };
    auto format(cst::Module const&, Format_configuration const&) -> std::string;
}
