#include <libutl/common/utilities.hpp>
#include <compiler/project.hpp>

namespace {

    constexpr auto trim(std::string_view string) -> std::string_view
    {
        if (string.find_first_not_of(' ') != std::string_view::npos) {
            string.remove_prefix(string.find_first_not_of(' '));
        }
        if (!string.empty()) {
            string.remove_suffix(string.size() - string.find_last_not_of(' ') - 1);
        }
        return string;
    }

    static_assert(trim("       test   ") == "test");
    static_assert(trim("      test") == "test");
    static_assert(trim("test     ") == "test");
    static_assert(trim("test") == "test");
    static_assert(trim("     ") == "");
    static_assert(trim("") == "");

    constexpr auto remove_comments(std::string_view string) -> std::string_view
    {
        if (utl::Usize const offset = string.find("//"); offset != std::string_view::npos) {
            string.remove_suffix(string.size() - offset);
        }
        return string;
    }

    static_assert(remove_comments("test//test") == "test");
    static_assert(remove_comments("test/test") == "test/test");
    static_assert(remove_comments("test") == "test");
    static_assert(remove_comments("// test") == "");
    static_assert(remove_comments("//") == "");
    static_assert(remove_comments("") == "");

    constexpr auto allowed_keys = std::to_array<std::string_view>({
        "language version",
        "source directory",
        "stack capacity",
        "name",
        "version",
        "authors",
        "created",
    });

} // namespace

auto project::to_string(Configuration const& configuration) -> std::string
{
    std::string string;
    string.reserve(256);
    auto out = std::back_inserter(string);

    for (auto const& [key, value] : configuration) {
        out = std::format_to(out, "{}: ", key);
        if (value.has_value()) {
            out = std::format_to(out, "{}", *value);
        }
    }

    return string;
}

auto project::default_configuration() -> Configuration
{
    Configuration configuration;
    configuration.add_new_or_abort("language version"s, "0"s);
    configuration.add_new_or_abort("source directory"s, "src"s);
    configuration.add_new_or_abort("stack capacity"s, "1048576 // 2^20"s);
    configuration.add_new_or_abort("name"s, tl::nullopt);
    configuration.add_new_or_abort("version"s, tl::nullopt);
    configuration.add_new_or_abort("authors"s, tl::nullopt);
    configuration.add_new_or_abort("created"s, "{:%d-%m-%Y}"_format(utl::local_time()));
    return configuration;
}

auto project::read_configuration() -> Configuration
{
    auto configuration_path = std::filesystem::current_path() / "kieli_config";

    Configuration configuration;

    if (std::ifstream file { configuration_path }) {
        auto       line        = utl::string_with_capacity(50);
        utl::Usize line_number = 0;

        while (std::getline(file, line)) {
            ++line_number;

            if (trim(line).empty()) {
                continue;
            }

            auto const mkview = [](auto&&) -> std::string_view { utl::todo(); };

            std::vector<std::string_view> components
                = line | ranges::views::split(':')
                | ranges::views::transform(utl::compose(&trim, &remove_comments, mkview))
                | ranges::to<std::vector>();

            switch (components.size()) {
            case 1:
                throw utl::exception(
                    "kieli_config: Expected a ':' after the key '{}'", components.front());
            case 2:
                break;
            default:
                throw utl::exception(
                    "kieli_config: Only one ':' is allowed per line: '{}'", trim(line));
            }

            std::string_view const key   = components.front();
            std::string_view const value = components.back();

            if (key.empty()) {
                throw utl::exception(
                    "kieli_config: empty key on the {} line",
                    utl::formatting::integer_with_ordinal_indicator(line_number));
            }

            if (!ranges::contains(allowed_keys, key)) {
                throw utl::exception(
                    "kieli_config: '{}' is not a recognized configuration key", key);
            }

            if (configuration.find(key) != nullptr) {
                throw utl::exception(
                    "kieli_config: '{}' key redefinition on the {} line",
                    key,
                    utl::formatting::integer_with_ordinal_indicator(line_number));
            }

            configuration.add_or_assign(
                std::string(key),
                value.empty() ? tl::make_optional(std::string(value)) : tl::nullopt);
        }

        return configuration;
    }

    return default_configuration();
}

auto project::initialize(std::string_view const project_name) -> void
{
    auto parent_path  = std::filesystem::current_path();
    auto project_path = parent_path / project_name;
    auto source_dir   = project_path / "src";

    if (project_path.has_extension()) {
        throw utl::exception("A directory name can not have a file extension");
    }

    if (is_directory(project_path)) {
        throw utl::exception(
            "A directory with the path '{}' already exists. Please use a new name",
            project_path.string());
    }

    if (!create_directory(project_path)) {
        throw utl::exception(
            "Could not create a directory with the path '{}'", project_path.string());
    }

    if (std::ofstream configuration_file { project_path / "kieli_config" }) {
        configuration_file << project::to_string(project::default_configuration());
    }
    else {
        throw utl::exception("Could not create the configuration file");
    }

    if (!create_directory(source_dir)) {
        throw utl::exception("Could not create the source directory");
    }

    if (std::ofstream main_file { source_dir / "main.kieli" }) {
        main_file << "import std\n\nfn main() {\n    print(\"Hello, world!\\n\")\n}";
    }
    else {
        throw utl::exception("Could not create the main file");
    }

    utl::print("Successfully created a new project at '{}'\n", project_path.string());
}
