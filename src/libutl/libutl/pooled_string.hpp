#pragma once

#include <libutl/utilities.hpp>

namespace utl {
    struct Relative_string {
        std::size_t offset {};
        std::size_t length {};

        [[nodiscard]] auto view_in(std::string_view) const -> std::string_view;

        template <class... Args>
        static auto format_to(
            std::string&                      out,
            std::format_string<Args...> const fmt,
            Args&&... args) -> Relative_string
        {
            auto const old_size = out.size();
            std::format_to(std::back_inserter(out), fmt, std::forward<Args>(args)...);
            return { .offset = old_size, .length = out.size() - old_size };
        }
    };

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
