#include <libutl/common/utilities.hpp>
#include <libutl/readline/readline.hpp>
#include <cpputil/io.hpp>

#if !__has_include(<readline/readline.h>) || !__has_include(<readline/history.h>)

auto utl::readline(char const* const prompt) -> std::optional<std::string>
{
    if (prompt) {
        (void)cpputil::io::write(stdout, prompt);
        (void)std::fflush(stdout);
    }
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

    auto environment_defined_path(char const* const name) -> std::optional<std::filesystem::path>
    {
        char const* const pointer = std::getenv(name); // NOLINT: thread unsafe
        return pointer ? std::optional(pointer) : std::nullopt;
    }

    auto xdg_state_home_fallback() -> std::optional<std::filesystem::path>
    {
        return environment_defined_path("HOME").transform(
            [](auto const& home) { return home / ".local" / "state"; });
    }

    auto xdg_state_home() -> std::optional<std::filesystem::path>
    {
        return environment_defined_path("XDG_STATE_HOME").or_else(xdg_state_home_fallback);
    }

    auto default_history_file_path() -> std::optional<std::filesystem::path>
    {
        return xdg_state_home().transform(
            [](auto const& directory) { return directory / "kieli_history"; });
    }

    auto history_file_path() -> std::optional<std::filesystem::path>
    {
        return environment_defined_path("KIELI_HISTORY").or_else(default_history_file_path);
    }

    auto read_history_file_to_current_history() -> void
    {
        auto const path = history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(path.value())) {
            return;
        }
        if (auto const file = cpputil::io::File::open_read(path.value().c_str())) {
            for (std::string const& line : cpputil::io::lines(file.get())) {
                previous_readline_input = line;
                ::add_history(line.c_str());
            }
        }
    }

    auto add_line_to_history_file(std::string_view const line) -> void
    {
        auto const path = history_file_path();
        if (!path.has_value() || !is_valid_history_file_path(path.value())) {
            return;
        }
        if (auto const file = cpputil::io::File::open_append(path.value().c_str())) {
            (void)cpputil::io::write_line(file.get(), line);
        }
    }

    auto underlying_readline(char const* const prompt)
    {
        using Free_fn = decltype([](void* const ptr) { std::free(ptr); }); // NOLINT: manual free
        return std::unique_ptr<char, Free_fn> { ::readline(prompt) };
    }
} // namespace

auto utl::readline(char const* const prompt) -> std::optional<std::string>
{
    [[maybe_unused]] static auto const _ = (read_history_file_to_current_history(), 0);

    auto const input = underlying_readline(prompt);
    return input ? std::optional(input.get()) : std::nullopt;
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
