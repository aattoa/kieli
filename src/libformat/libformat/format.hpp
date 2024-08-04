#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/cst/cst.hpp>

namespace kieli {

    enum class Format_function_body { leave_as_is, normalize_to_block, normalize_to_equals_sign };

    struct Format_configuration {
        std::size_t          block_indentation         = 4;
        std::size_t          space_between_definitions = 1;
        Format_function_body function_body             = Format_function_body::leave_as_is;
    };

    auto format_module(CST::Module const&, Format_configuration const&) -> std::string;
    auto format_definition(cst::Definition const&, Format_configuration const&) -> std::string;
    auto format_expression(cst::Expression const&, Format_configuration const&) -> std::string;
    auto format_pattern(cst::Pattern const&, Format_configuration const&) -> std::string;
    auto format_type(cst::Type const&, Format_configuration const&) -> std::string;

} // namespace kieli
