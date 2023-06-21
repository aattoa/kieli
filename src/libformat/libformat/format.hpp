#pragma once

#include <libutl/common/utilities.hpp>
#include <libparse/cst.hpp>


namespace kieli {
    struct Format_configuration {
        tl::optional<utl::Usize> block_indentation;
    };
    auto format(cst::Module const&, Format_configuration const&) -> std::string;
}
