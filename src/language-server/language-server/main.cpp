#include <libutl/utilities.hpp>
#include <language-server/server.hpp>

using namespace ki;

auto main(int argc, char const* const* argv) -> int
{
    try {
        auto config = lsp::default_server_config();
        if (std::find(argv, argv + argc, "--debug"sv) != (argv + argc)) {
            config.log_level = db::Log_level::Debug;
        }
        return lsp::run_server(std::move(config), std::cin, std::cout);
    }
    catch (std::exception const& exception) {
        std::println(std::cerr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
