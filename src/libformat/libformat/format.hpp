#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli {

    enum class Format_function_body { leave_as_is, normalize_to_block, normalize_to_equals_sign };

    struct Format_options {
        std::size_t          tab_size                        = 4;
        bool                 use_spaces                      = true;
        std::size_t          empty_lines_between_definitions = 1;
        Format_function_body function_body                   = Format_function_body::leave_as_is;
    };

    auto format_module(CST::Module const& module, Format_options const& options) -> std::string;

    auto format(
        cst::Arena const&      arena,
        Format_options const&  options,
        cst::Definition const& definition,
        std::string&           output) -> void;

    auto format(
        cst::Arena const&      arena,
        Format_options const&  options,
        cst::Expression const& expression,
        std::string&           output) -> void;

    auto format(
        cst::Arena const&     arena,
        Format_options const& options,
        cst::Pattern const&   pattern,
        std::string&          output) -> void;

    auto format(
        cst::Arena const&     arena,
        Format_options const& options,
        cst::Type const&      type,
        std::string&          output) -> void;

    auto format(
        cst::Arena const&     arena,
        Format_options const& options,
        cst::Expression_id    expression_id,
        std::string&          output) -> void;

    auto format(
        cst::Arena const&     arena,
        Format_options const& options,
        cst::Pattern_id       pattern_id,
        std::string&          output) -> void;

    auto format(
        cst::Arena const&     arena,
        Format_options const& options,
        cst::Type_id          type_id,
        std::string&          output) -> void;

    template <class T>
    concept formattable = requires(
        cst::Arena const arena, Format_options const options, T const object, std::string output) {
        { kieli::format(arena, options, object, output) } -> std::same_as<void>;
    };

    auto to_string(
        cst::Arena const&       arena,
        Format_options const&   options,
        formattable auto const& object) -> std::string
    {
        std::string output;
        kieli::format(arena, options, object, output);
        return output;
    }

} // namespace kieli
