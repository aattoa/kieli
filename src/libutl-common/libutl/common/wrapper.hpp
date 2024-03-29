#pragma once

#include <libutl/common/utilities.hpp>

namespace utl::dtl {
    template <class T>
    class [[nodiscard]] Wrapper_arena_page {
        std::size_t           m_page_size;
        T*                    m_buffer;
        T*                    m_slot;
        static constexpr auto alignment = static_cast<std::align_val_t>(alignof(T));
    public:
        explicit Wrapper_arena_page(std::size_t const page_size)
            : m_page_size { page_size }
            , m_buffer { static_cast<T*>(::operator new(sizeof(T) * page_size, alignment)) }
            , m_slot { m_buffer }
        {}

        Wrapper_arena_page(Wrapper_arena_page&& other) noexcept
            : m_page_size { std::exchange(other.m_page_size, 0) }
            , m_buffer { std::exchange(other.m_buffer, nullptr) }
            , m_slot { std::exchange(other.m_slot, nullptr) }
        {}

        auto operator=(Wrapper_arena_page&& other) noexcept -> Wrapper_arena_page&
        {
            if (this != &other) {
                std::destroy_at(this);
                std::construct_at(this, std::move(other));
            }
            return *this;
        }

        ~Wrapper_arena_page()
        {
            if (m_buffer) {
                std::destroy(m_buffer, m_slot);
                ::operator delete(m_buffer, sizeof(T) * m_page_size, alignment);
            }
        }

        Wrapper_arena_page(Wrapper_arena_page const&) = delete;
        auto operator=(Wrapper_arena_page const&)     = delete;

        [[nodiscard]] auto is_at_capacity() const noexcept -> bool
        {
            return m_slot == m_buffer + m_page_size;
        }

        template <class... Args>
        [[nodiscard]] auto unsafe_emplace_back(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args&&...>) -> T*
        {
            assert(!is_at_capacity());
            std::construct_at(m_slot, std::forward<Args>(args)...);
            return m_slot++;
        }
    };

    template <class>
    struct Is_wrapper : std::false_type {};
} // namespace utl::dtl

namespace utl {

    enum class Wrapper_mutability { yes, no };

    template <class T, Wrapper_mutability = Wrapper_mutability::no>
    class [[nodiscard]] Wrapper;

    template <class...>
    class [[nodiscard]] Wrapper_arena;

    template <class T>
    class [[nodiscard]] Wrapper_arena<T> {
        using Page = dtl::Wrapper_arena_page<T>;
        std::vector<Page> m_pages;
        std::size_t       m_page_size;

        explicit constexpr Wrapper_arena(std::size_t const page_size) noexcept
            : m_page_size { page_size }
        {
            cpputil::always_assert(page_size != 0);
        }
    public:
        static constexpr auto with_page_size(std::size_t const page_size) -> Wrapper_arena
        {
            return Wrapper_arena { page_size };
        }

        static constexpr auto with_default_page_size() -> Wrapper_arena
        {
            return with_page_size(256);
        }

        template <Wrapper_mutability mut = Wrapper_mutability::no, class... Args>
        auto wrap(Args&&... args) -> Wrapper<T, mut>
            requires std::is_constructible_v<T, Args...>
        {
            auto const it   = std::ranges::find_if_not(m_pages, &Page::is_at_capacity);
            Page&      page = it != m_pages.end() ? *it : m_pages.emplace_back(m_page_size);
            return Wrapper<T, mut> { page.unsafe_emplace_back(std::forward<Args>(args)...) };
        }

        template <class... Args>
        auto wrap_mutable(Args&&... args) -> Wrapper<T, Wrapper_mutability::yes>
            requires std::is_constructible_v<T, Args...>
        {
            return wrap<Wrapper_mutability::yes>(std::forward<Args>(args)...);
        }

        auto merge_with(Wrapper_arena&& other) & -> void
        {
            m_pages.append_range(std::views::as_rvalue(other.m_pages));
        }
    };

    template <class... Ts>
    class [[nodiscard]] Wrapper_arena : Wrapper_arena<Ts>... {
    public:
        explicit Wrapper_arena(Wrapper_arena<Ts>&&... arenas) noexcept
            : Wrapper_arena<Ts> { std::move(arenas) }...
        {}

        static auto with_page_size(std::size_t const page_size) -> Wrapper_arena
        {
            return Wrapper_arena { Wrapper_arena<Ts>::with_page_size(page_size)... };
        }

        static auto with_default_page_size() -> Wrapper_arena
        {
            return Wrapper_arena { Wrapper_arena<Ts>::with_default_page_size()... };
        }

        template <one_of<Ts...> T, Wrapper_mutability mut = Wrapper_mutability::no, class... Args>
        auto wrap(Args&&... args) -> Wrapper<T, mut>
            requires std::is_constructible_v<T, Args...>
        {
            return Wrapper_arena<T>::template wrap<mut>(std::forward<Args>(args)...);
        }

        template <one_of<Ts...> T, class... Args>
        auto wrap_mutable(Args&&... args) -> Wrapper<T, Wrapper_mutability::yes>
            requires std::is_constructible_v<T, Args...>
        {
            return Wrapper_arena<T>::wrap_mutable(std::forward<Args>(args)...);
        }

        auto merge_with(Wrapper_arena&& other) & -> void
        {
            (Wrapper_arena<Ts>::merge_with(static_cast<Wrapper_arena<Ts>&&>(other)), ...);
        }
    };

    template <class T, Wrapper_mutability mut>
    class [[nodiscard]] Wrapper {
        T* m_pointer;

        explicit Wrapper(T* const pointer) noexcept : m_pointer { pointer } {}

        static constexpr bool is_mutable = (mut == Wrapper_mutability::yes);

        friend Wrapper_arena<T>;
    public:
        [[nodiscard]] auto operator*(this Wrapper const self) noexcept -> T const&
        {
            return *self.m_pointer;
        }

        [[nodiscard]] auto operator->(this Wrapper const self) noexcept -> T const*
        {
            return self.m_pointer;
        }

        [[nodiscard]] auto is(this Wrapper const self, Wrapper const other) noexcept -> bool
        {
            return self.m_pointer == other.m_pointer;
        }

        [[nodiscard]] auto is_not(this Wrapper const self, Wrapper const other) noexcept -> bool
        {
            return self.m_pointer != other.m_pointer;
        }

        [[nodiscard]] auto as_mutable(this Wrapper const self) noexcept -> T&
            requires is_mutable
        {
            return *self.m_pointer;
        }
    };

    template <class T>
    using Mutable_wrapper = Wrapper<T, Wrapper_mutability::yes>;

    template <class T>
    concept wrapper = dtl::Is_wrapper<T>::value;

} // namespace utl

template <class T, utl::Wrapper_mutability mut>
struct utl::dtl::Is_wrapper<utl::Wrapper<T, mut>> : std::true_type {};

template <class T, utl::Wrapper_mutability mut>
struct std::formatter<utl::Wrapper<T, mut>> : formatter<T> {
    auto format(utl::Wrapper<T, mut> const wrapper, auto& context) const
    {
        return formatter<T>::format(*wrapper, context);
    }
};
