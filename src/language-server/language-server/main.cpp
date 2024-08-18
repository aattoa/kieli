#include <libutl/utilities.hpp>
#include <language-server/server.hpp>
#include <iostream>

auto main(int const argc, char const* const* const argv) -> int
{
    bool const debug = std::find(argv, argv + argc, "--debug"sv) != (argv + argc);
    try {
        return kieli::lsp::run_server(debug, std::cin, std::cout);
    }
    catch (std::exception const& exception) {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
