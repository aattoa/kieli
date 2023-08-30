#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>

namespace project {

    using Configuration = utl::Flatmap<std::string, tl::optional<std::string>>;

    auto to_string(Configuration const&) -> std::string;

    auto default_configuration() -> Configuration;
    auto read_configuration() -> Configuration;

    auto initialize(std::string_view project_name) -> void;

} // namespace project
