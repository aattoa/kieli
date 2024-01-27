#pragma once

#include <libutl/common/utilities.hpp>
#include <libparse2/cst.hpp>

// TODO: collapse string literals, expand integer literals, insert digit separators

namespace kieli {
    struct Format_configuration {
        enum class Function_body { leave_as_is, normalize_to_block, normalize_to_equals_sign };
        std::size_t   block_indentation = 4;
        Function_body function_body     = Function_body::leave_as_is;
    };

    auto format_module(cst::Module const&, Format_configuration const&) -> std::string;
    auto format_definition(cst::Definition const&, Format_configuration const&) -> std::string;
    auto format_expression(cst::Expression const&, Format_configuration const&) -> std::string;
    auto format_pattern(cst::Pattern const&, Format_configuration const&) -> std::string;
    auto format_type(cst::Type const&, Format_configuration const&) -> std::string;
} // namespace kieli
