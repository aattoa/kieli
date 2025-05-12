#ifndef KIELI_LANGUAGE_SERVER_RPC
#define KIELI_LANGUAGE_SERVER_RPC

#include <libutl/utilities.hpp>

namespace ki::lsp::rpc {

    void write_message(std::ostream& out, std::string_view message);

    auto read_message(std::istream& in) -> std::expected<std::string, std::string_view>;

} // namespace ki::lsp::rpc

#endif // KIELI_LANGUAGE_SERVER_RPC
