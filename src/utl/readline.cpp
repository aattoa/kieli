#include "utl/utilities.hpp"
#include "readline.hpp"


#if defined(linux) && __has_include(<readline/readline.h>) && __has_include(<readline/history.h>)


#include <readline/readline.h>
#include <readline/history.h>

auto utl::readline(char const* const prompt) -> std::string {
    char* const input { ::readline(prompt) };
    auto const input_guard = on_scope_exit([=] { std::free(input); });

    if (!input || !*input)
        return {};

    ::add_history(input);
    return std::string { input };
}


#else


auto utl::readline(char const* const prompt) -> std::string {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}


#endif