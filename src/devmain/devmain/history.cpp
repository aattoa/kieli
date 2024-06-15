#include <libutl/utilities.hpp>
#include <devmain/history.hpp>
#include <cpputil/input.hpp>
#include <cpputil/io.hpp>

namespace {
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
        char const* const pointer = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
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

    thread_local std::string
        previous_history_line; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

auto kieli::add_to_history(char const* const line) -> void
{
    if (!line || previous_history_line == line) {
        return;
    }
    cpputil::input::add_history(line);
    add_line_to_history_file(line);
    previous_history_line = line;
}

auto kieli::read_history_file_to_active_history() -> void
{
    auto const path = history_file_path();
    if (!path.has_value() || !is_valid_history_file_path(path.value())) {
        return;
    }
    if (auto const file = cpputil::io::File::open_read(path.value().c_str())) {
        for (std::string const& line : cpputil::io::lines(file.get())) {
            previous_history_line = line;
            cpputil::input::add_history(line.c_str());
        }
    }
}
