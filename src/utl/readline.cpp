#include "utl/utilities.hpp"
#include "readline.hpp"


#if defined(linux) && __has_include(<readline/readline.h>) && __has_include(<readline/history.h>)


#include <readline/readline.h>
#include <readline/history.h>

namespace {
    std::string previous_readline_input;

    auto is_valid_history_file_path(std::filesystem::path const& path) -> bool {
        try {
            return !std::filesystem::exists(path)
                || std::filesystem::is_regular_file(path);
        }
        catch (std::filesystem::filesystem_error const&) {
            return false;
        }
    }

    auto determine_history_file_path() -> tl::optional<std::filesystem::path> {
        if (char const* const path_override = std::getenv("KIELI_HISTORY")) {
            return path_override;
        }
        else if (char const* const home = std::getenv("HOME")) {
            std::filesystem::path path = home;
            path /= ".kieli_history";
            return path;
        }
        return tl::nullopt;
    }

    auto read_history_file_to_current_history() -> void {
        tl::optional const path = determine_history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(*path))
            return;

        std::ifstream file { *path };
        if (!file.is_open())
            return;

        std::string line;
        while (std::getline(file, line)) {
            ::add_history(line.c_str());

            // `line` is cleared when getline fails, so this has to be done every time.
            previous_readline_input = line;
        }
    }

    auto add_line_to_history_file(std::string_view const line) -> void {
        tl::optional const path = determine_history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(*path))
            return;

        std::ofstream file { *path, std::ios_base::app };
        if (!file.is_open())
            return;

        file << line << '\n';
    }
}


auto utl::readline(std::string const& prompt) -> std::string {
    [[maybe_unused]]
    static auto const history_reader =
        (read_history_file_to_current_history(), 0);

    char* const raw_input   = ::readline(prompt.c_str());
    auto  const input_guard = on_scope_exit([=] { std::free(raw_input); });

    if (!raw_input || !*raw_input)
        return {};

    std::string input = raw_input;

    if (previous_readline_input != input) {
        ::add_history(input.c_str());
        add_line_to_history_file(input);
        previous_readline_input = input;
    }

    return input;
}


#else


auto utl::readline(std::string const& prompt) -> std::string {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}


#endif