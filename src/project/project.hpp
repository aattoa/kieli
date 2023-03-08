#pragma once

#include "utl/utilities.hpp"
#include "utl/flatmap.hpp"


namespace project {

    struct Configuration_key {
        std::string string;

        Configuration_key(std::string&& string) noexcept
            : string { std::move(string) } {}

        template <class T>
        auto parse() const -> std::optional<T> requires std::is_arithmetic_v<T> {
            if (string.empty()) {
                return std::nullopt;
            }
            T value;
            auto const [ptr, ec] = std::from_chars(&string.front(), &string.back(), value);
            return ec == std::errc {} ? value : std::optional<T> {};
        }
    };


    class [[nodiscard]] Configuration :
        utl::Flatmap<std::string, std::optional<Configuration_key>, utl::Flatmap_strategy::store_keys>
    {
    public:
        Configuration() = default;

        auto operator[](std::string_view) -> Configuration_key&;
        auto string() const -> std::string;

        using Flatmap::find;
        using Flatmap::add;
    };


    auto default_configuration() -> Configuration;

    auto read_configuration() -> Configuration;

}