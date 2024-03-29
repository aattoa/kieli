#pragma once

#include <libutl/common/utilities.hpp>

namespace utl {
    class [[nodiscard]] String_pool;
    class [[nodiscard]] Pooled_string;

    [[nodiscard]] auto operator==(Pooled_string const&, std::string_view) -> bool;
    [[nodiscard]] auto operator==(std::string_view, Pooled_string const&) -> bool;
} // namespace utl

class utl::String_pool {
    std::unique_ptr<std::string> m_string;
public:
    String_pool();

    static auto with_initial_capacity(std::size_t capacity) -> String_pool;

    auto make(std::string_view) -> Pooled_string;
    auto make_guaranteed_new_string(std::string_view) -> Pooled_string;
};

class utl::Pooled_string {
    friend String_pool;
    Relative_string    m_relative;
    std::string const* m_pool;
    explicit Pooled_string(Relative_string relative, std::string const* pool);
public:
    [[nodiscard]] auto view() const noexcept -> std::string_view;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto operator==(Pooled_string const&) const -> bool;
};

template <>
struct std::formatter<utl::Pooled_string> : std::formatter<std::string_view> {
    auto format(utl::Pooled_string const string, auto& context) const
    {
        return std::formatter<std::string_view>::format(string.view(), context);
    }
};
