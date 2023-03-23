#pragma once

#include "utl/utilities.hpp"
#include "utl/flatmap.hpp"


namespace project {

    struct Configuration_key {
        std::string string;

        Configuration_key(std::string&& string) noexcept
            : string { std::move(string) } {}

        [[nodiscard]]
        auto operator==(Configuration_key const&) const noexcept -> bool = default;

        template <class T>
        auto parse() const -> tl::optional<T> requires std::is_arithmetic_v<T> {
            if (string.empty())
                return tl::nullopt;

            T value;
            auto const [ptr, ec] = std::from_chars(&string.front(), &string.back(), value);
            return ec == std::errc {} ? value : tl::optional<T> {};
        }
    };


    class [[nodiscard]] Configuration :
        utl::Flatmap<std::string, tl::optional<Configuration_key>, utl::Flatmap_strategy::store_keys>
    {
    public:
        Configuration() = default;

        [[nodiscard]]
        auto operator[](std::string_view) -> Configuration_key&;
        [[nodiscard]]
        auto string() const -> std::string;

        using Flatmap::find;
        using Flatmap::add;
    };


    auto default_configuration() -> Configuration;

    auto read_configuration() -> Configuration;

}
