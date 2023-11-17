#include <libutl/common/utilities.hpp>
#include <libutl/readline/readline.hpp>

#if !__has_include(<readline/readline.h>) || !__has_include(<readline/history.h>)

auto utl::readline(std::string const& prompt) -> std::string
{
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

auto utl::add_to_readline_history(std::string const&) -> void {}

#else

#include <readline/readline.h>
#include <readline/history.h>

namespace {
    thread_local std::string previous_readline_input; // NOLINT: mutable global

    auto is_valid_history_file_path(std::filesystem::path const& path) -> bool
    {
        try {
            return !std::filesystem::exists(path) || std::filesystem::is_regular_file(path);
        }
        catch (std::filesystem::filesystem_error const&) {
            return false;
        }
    }

    auto xdg_state_home_filename() -> std::optional<std::filesystem::path>
    {
        static constexpr auto filename = ".kieli_history"sv;
        if (char const* const state_home = std::getenv("XDG_STATE_HOME")) { // NOLINT
            return std::filesystem::path { state_home } / filename;
        }
        if (char const* const home = std::getenv("HOME")) { // NOLINT
            return std::filesystem::path { home } / ".local" / "state" / filename;
        }
        return std::nullopt;
    }

    auto determine_history_file_path() -> std::optional<std::filesystem::path>
    {
        if (char const* const override = std::getenv("KIELI_HISTORY")) { // NOLINT
            return override;
        }
        return xdg_state_home_filename();
    }

    auto read_history_file_to_current_history() -> void
    {
        std::optional const path = determine_history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(*path)) {
            return;
        }

        std::ifstream file { *path };
        if (!file.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            ::add_history(line.c_str());
            // `line` is cleared when getline fails, so previous input has to be set every time.
            previous_readline_input = line;
        }
    }

    auto add_line_to_history_file(std::string_view const line) -> void
    {
        std::optional const path = determine_history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(*path)) {
            return;
        }

        std::ofstream file { *path, std::ios_base::app };
        if (!file.is_open()) {
            return;
        }

        file << line << '\n';
    }
} // namespace

auto utl::readline(std::string const& prompt) -> std::string
{
    [[maybe_unused]] static auto const history_reader = (read_history_file_to_current_history(), 0);

    char* const raw_input   = ::readline(prompt.c_str());
    auto const  input_guard = on_scope_exit([=] { std::free(raw_input); }); // NOLINT: manual free
    return raw_input ? raw_input : std::string {};
}

auto utl::add_to_readline_history(std::string const& string) -> void
{
    if (previous_readline_input == string) {
        return;
    }
    ::add_history(string.c_str());
    add_line_to_history_file(string);
    previous_readline_input = string;
}

#endif
