#pragma once

#include "utl/utilities.hpp"


namespace utl {

    template <class Tag>
    class [[nodiscard]] String_pool;

    template <class Tag>
    class [[nodiscard]] Pooled_string {
    public:
        friend class String_pool<Tag>;
        using Pool = String_pool<Tag>;
    private:
        Usize              m_index;
        Usize              m_size;
        std::string const* m_pool;

        explicit constexpr Pooled_string(Usize const index, Usize const size, std::string const* const pool) // NOLINT
            : m_index { index }, m_size { size }, m_pool { pool } {}
    public:
        [[nodiscard]]
        constexpr auto view() const noexcept -> std::string_view {
            return { m_pool->c_str() + m_index, m_size };
        }
        [[nodiscard]]
        constexpr auto size() const noexcept -> Usize {
            return m_size;
        }
        [[nodiscard]]
        auto operator==(Pooled_string const&) const noexcept -> bool = default;
    };


    template <class Tag>
    class [[nodiscard]] String_pool {
        friend class Pooled_string<Tag>;
        std::unique_ptr<std::string> m_string;
    public:
        constexpr String_pool()
            : m_string { std::make_unique<std::string>(string_with_capacity(2048)) } {}

        constexpr auto make(std::string_view const string) -> Pooled_string<Tag> {
            // The searcher overload is faster than the bare iterator overload for some reason
            auto it = std::search(m_string->begin(), m_string->end(), std::default_searcher { string.begin(), string.end() });

            if (it == m_string->end())
                return make_guaranteed_new_string(string);
            else
                return Pooled_string<Tag> { unsigned_distance(m_string->begin(), it), string.size(), m_string.get() };
        }
        constexpr auto make_guaranteed_new_string(std::string_view const string) -> Pooled_string<Tag> {
            Usize const index = m_string->size();
            m_string->append(string);
            return Pooled_string<Tag> { index, string.size(), m_string.get() };
        }
    };

}


template <class Tag>
struct fmt::formatter<utl::Pooled_string<Tag>> : fmt::formatter<std::string_view> {
    auto format(utl::Pooled_string<Tag> const string, auto& context) {
        return fmt::formatter<std::string_view>::format(string.view(), context);
    }
};