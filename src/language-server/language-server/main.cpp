#include <libutl/utilities.hpp>
#include <language-server/server.hpp>

using namespace ki;

auto main() -> int
{
    try {
        return lsp::run_server(lsp::default_server_config(), std::cin, std::cout);
    }
    catch (std::exception const& exception) {
        std::println(std::cerr, "Error: Unhandled exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
