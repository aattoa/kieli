#ifndef KIELI_LANGUAGE_SERVER_SERVER
#define KIELI_LANGUAGE_SERVER_SERVER

#include <iosfwd>

namespace ki::lsp {

    // Run a language server with the given I/O streams.
    [[nodiscard]] auto run_server(bool debug, std::istream& in, std::ostream& out) -> int;

} // namespace ki::lsp

#endif // KIELI_LANGUAGE_SERVER_SERVER
