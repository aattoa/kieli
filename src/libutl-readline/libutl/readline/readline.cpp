#include <libutl/common/utilities.hpp>
#include <libutl/readline/readline.hpp>
#include <cpputil/io.hpp>

#if !__has_include(<readline/readline.h>) || !__has_include(<readline/history.h>)

auto utl::readline(std::string const& prompt) -> std::optional<std::string>
{
    (void)cpputil::io::write(stdout, prompt);
    (void)std::fflush(stdout);
    return cpputil::io::read_line(stdin);
}

auto utl::add_to_readline_history(std::string const&) -> void
{
    // Do nothing
}

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
        static constexpr std::string_view filename = "kieli_history";
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
        if (!path.has_value() || !is_valid_history_file_path(path.value())) {
            return;
        }
        auto file = cpputil::io::File::open_read(path.value().c_str());
        if (!file) {
            return;
        }
        std::string line;
        while (cpputil::io::read_line(file.get(), line)) {
            ::add_history(line.c_str());
            previous_readline_input = line;
            line.clear();
        }
    }

    auto add_line_to_history_file(std::string_view const line) -> void
    {
        std::optional const path = determine_history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(path.value())) {
            return;
        }
        if (auto file = cpputil::io::File::open_append(path.value().c_str())) {
            (void)cpputil::io::write_line(file.get(), line);
        }
    }
} // namespace

auto utl::readline(std::string const& prompt) -> std::optional<std::string>
{
    [[maybe_unused]] static auto const _ = (read_history_file_to_current_history(), 0);
    using Free_fn = decltype([](void* const ptr) { std::free(ptr); }); // NOLINT: manual free
    if (std::unique_ptr<char, Free_fn> const input { ::readline(prompt.c_str()) }) {
        return input.get();
    }
    return std::nullopt;
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
