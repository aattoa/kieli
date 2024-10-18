#pragma once

#include <libutl/utilities.hpp>
#include <cpputil/json.hpp>
#include <iosfwd>

namespace kieli::lsp {

    enum struct Read_failure { no_header, no_length, no_separator, no_content };

    void rpc_write_message(std::ostream& out, std::string_view message);

    auto rpc_read_message(std::istream& in) -> std::expected<std::string, Read_failure>;

} // namespace kieli::lsp
