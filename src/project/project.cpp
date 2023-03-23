#include "utl/utilities.hpp"
#include "language/configuration.hpp"
#include "project.hpp"


namespace {

    constexpr auto trim(std::string_view string) -> std::string_view {
        if (string.find_first_not_of(' ') != std::string_view::npos) {
            string.remove_prefix(string.find_first_not_of(' '));
        }
        if (!string.empty()) {
            string.remove_suffix(string.size() - string.find_last_not_of(' ') - 1);
        }
        return string;
    }

    static_assert(trim("       test   ") == "test");
    static_assert(trim("      test"    ) == "test");
    static_assert(trim("test     "     ) == "test");
    static_assert(trim("test"          ) == "test");
    static_assert(trim("     "         ) == ""    );
    static_assert(trim(""              ) == ""    );


    constexpr auto remove_comments(std::string_view string) -> std::string_view {
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
        "created"
    });

}


auto project::Configuration::string() const -> std::string {
    std::string string;
    string.reserve(256);
    auto out = std::back_inserter(string);

    for (auto const& [key, value] : span()) {
        out = fmt::format_to(out, "{}: ", key);
        if (value) {
            out = fmt::format_to(out, "{}", value->string);
        }
    }

    return string;
}


auto project::Configuration::operator[](std::string_view const name) -> Configuration_key& {
    utl::always_assert(ranges::contains(allowed_keys, name));

    if (auto* const key = find(name); key && *key) {
        return **key;
    }
    else {
        utl::abort();
    }
}


auto project::default_configuration() -> Configuration {
    Configuration configuration;
    configuration.add("language version", std::to_string(language::version));
    configuration.add("source directory", "src"s);
    configuration.add("stack capacity"  , "1048576 // 2^20"s);
    configuration.add("name"            , tl::nullopt);
    configuration.add("version"         , tl::nullopt);
    configuration.add("authors"         , tl::nullopt);
    configuration.add("created"         , "{:%d-%m-%Y}"_format(utl::local_time()));
    return configuration;
}


auto project::read_configuration() -> Configuration {
    auto configuration_path = std::filesystem::current_path() / "kieli_config";

    Configuration configuration;

    if (std::ifstream file { configuration_path }) {
        std::string line;
        line.reserve(50);

        utl::Usize line_number = 0;

        while (std::getline(file, line)) {
            ++line_number;

            if (trim(line).empty()) {
                continue;
            }

            auto const mkview = [](auto&&) -> std::string_view { utl::todo(); };

            std::vector<std::string_view> components
                = line
                | ranges::views::split(':')
                | ranges::views::transform(utl::compose(&trim, &remove_comments, mkview))
                | ranges::to<std::vector>();

            switch (components.size()) {
            case 1:
                throw utl::exception("kieli_config: Expected a ':' after the key '{}'", components.front());
            case 2:
                break;
            default:
                throw utl::exception("kieli_config: Only one ':' is allowed per line: '{}'", trim(line));
            }

            std::string_view const key = components.front();
            std::string_view const value = components.back();

            if (key.empty()) {
                throw utl::exception(
                    "kieli_config: empty key on the {} line",
                    utl::formatting::integer_with_ordinal_indicator(line_number)
                );
            }

            if (!ranges::contains(allowed_keys, key)) {
                throw utl::exception(
                    "kieli_config: '{}' is not a recognized configuration key",
                    key
                );
            }

            if (configuration.find(key)) {
                throw utl::exception(
                    "kieli_config: '{}' key redefinition on the {} line",
                    key,
                    utl::formatting::integer_with_ordinal_indicator(line_number)
                );
            }

            configuration.add(
                std::string(key),
                value.empty()
                    ? tl::optional(std::string(value))
                    : tl::nullopt
            );
        }

        return configuration;
    }
    else {
        return default_configuration();
    }
}
