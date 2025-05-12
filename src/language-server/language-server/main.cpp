#include <libutl/utilities.hpp>
#include <language-server/server.hpp>

auto main(int argc, char const* const* argv) -> int
{
    bool const debug = std::find(argv, argv + argc, "--debug"sv) != (argv + argc);
    try {
        return ki::lsp::run_server(debug, std::cin, std::cout);
    }
    catch (std::exception const& exception) {
        std::println(std::cerr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
