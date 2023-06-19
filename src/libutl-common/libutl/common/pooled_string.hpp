#pragma once

#include <libutl/common/utilities.hpp>


namespace utl {
    class [[nodiscard]] String_pool;

    class [[nodiscard]] Pooled_string {
    public:
        friend struct std::hash<Pooled_string>;
        friend class String_pool;
    private:
        Usize              m_index;
        Usize              m_size;
        std::string const* m_pool;
        explicit Pooled_string(Usize index, Usize size, std::string const* pool); // NOLINT
    public:
        [[nodiscard]] auto view() const noexcept -> std::string_view;
        [[nodiscard]] auto size() const noexcept -> Usize;
        [[nodiscard]] auto operator==(Pooled_string const&) const -> bool = default;
    };

    class [[nodiscard]] String_pool {
        std::unique_ptr<std::string> m_string;
    public:
        explicit String_pool(Usize initial_capacity);
        String_pool();
        auto make                      (std::string_view) -> Pooled_string;
        auto make_guaranteed_new_string(std::string_view) -> Pooled_string;
    };

    [[nodiscard]] auto operator==(Pooled_string const&, std::string_view) -> bool;
    [[nodiscard]] auto operator==(std::string_view, Pooled_string const&) -> bool;
}


template <>
struct std::formatter<utl::Pooled_string> : std::formatter<std::string_view> {
    auto format(utl::Pooled_string const string, auto& context) const {
        return std::formatter<std::string_view>::format(string.view(), context);
    }
};

template <>
struct std::hash<utl::Pooled_string> {
    auto operator()(utl::Pooled_string const string) const noexcept -> utl::Usize {
        return string.m_index;
    }
};
