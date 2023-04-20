#pragma once

#include "utl/utilities.hpp"


namespace utl::dtl {
    template <class T>
    class [[nodiscard]] Wrapper_arena_page {
        Usize m_page_size;
        T*    m_buffer;
        T*    m_slot;
        static constexpr auto alignment = static_cast<std::align_val_t>(alignof(T));
    public:
        explicit Wrapper_arena_page(Usize const page_size)
            : m_page_size { page_size }
            , m_buffer    { static_cast<T*>(::operator new(sizeof(T) * page_size, alignment)) }
            , m_slot      { m_buffer } {}

        Wrapper_arena_page(Wrapper_arena_page&& other) noexcept
            : m_page_size { std::exchange(other.m_page_size, 0) }
            , m_buffer    { std::exchange(other.m_buffer, nullptr) }
            , m_slot      { std::exchange(other.m_slot, nullptr) } {}

        auto operator=(Wrapper_arena_page&& other) noexcept -> Wrapper_arena_page& {
            if (this != &other) {
                std::destroy_at(this);
                std::construct_at(this, std::move(other));
            }
            return *this;
        }

        ~Wrapper_arena_page() {
            if (!m_buffer) return;
            std::destroy(m_buffer, m_slot);
            ::operator delete(m_buffer, sizeof(T) * m_page_size, alignment);
        }

        Wrapper_arena_page(Wrapper_arena_page const&) = delete;
        auto operator=    (Wrapper_arena_page const&) = delete;

        [[nodiscard]]
        auto is_at_capacity() const noexcept -> bool {
            return m_slot == m_buffer + m_page_size;
        }
        template <class... Args> [[nodiscard]]
        auto unsafe_emplace_arena_element(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args&&...>) -> T*
        {
            assert(!is_at_capacity());
            std::construct_at(m_slot, std::forward<Args>(args)...);
            return m_slot++;
        }
    };
}


namespace utl {

    template <class T>
    class [[nodiscard]] Wrapper;
    template <class...>
    class [[nodiscard]] Wrapper_arena;

    template <class T>
    class [[nodiscard]] Wrapper_arena<T> {
        using Page = dtl::Wrapper_arena_page<T>;
        std::vector<Page> m_pages;
        Usize             m_page_size;
        Usize             m_size = 0;
        explicit Wrapper_arena(Usize const page_size) noexcept
            : m_page_size { page_size } {}
    public:
        static auto with_page_size(Usize const page_size) -> Wrapper_arena<T> {
            return Wrapper_arena<T> { page_size };
        }
        static auto with_default_page_size() -> Wrapper_arena<T> {
            return with_page_size(1024);
        }
        template <class... Args>
        auto wrap(Args&&... args) -> Wrapper<T> {
            auto const it = ranges::find_if_not(m_pages, &Page::is_at_capacity);
            Page& page = it != m_pages.end() ? *it : m_pages.emplace_back(m_page_size);
            T* const element = page.unsafe_emplace_arena_element(std::forward<Args>(args)...);
            ++m_size;
            return Wrapper<T> { element };
        }
        auto merge_with(Wrapper_arena&& other) & -> void {
            m_pages.insert(
                m_pages.end(),
                std::move_iterator { other.m_pages.begin() },
                std::move_iterator { other.m_pages.end() });
        }
    };

    template <class... Ts>
    class [[nodiscard]] Wrapper_arena : Wrapper_arena<Ts>... {
    public:
        explicit Wrapper_arena(Wrapper_arena<Ts>&&... arenas) noexcept
            : Wrapper_arena<Ts> { std::move(arenas) }... {}

        static auto with_page_size(Usize const page_size) -> Wrapper_arena {
            return Wrapper_arena { Wrapper_arena<Ts>::with_page_size(page_size)... };
        }
        static auto with_default_page_size() -> Wrapper_arena {
            return Wrapper_arena { Wrapper_arena<Ts>::with_default_page_size()... };
        }

        template <one_of<Ts...> T, class... Args>
        auto wrap(Args&&... args) -> Wrapper<T> {
            return Wrapper_arena<T>::wrap(std::forward<Args>(args)...);
        }
        template <class Arg>
        auto wrap(Arg&& arg) -> utl::Wrapper<std::remove_cvref_t<Arg>>
            requires utl::one_of<std::remove_cvref_t<Arg>, Ts...>
        {
            return Wrapper_arena<std::remove_cvref_t<Arg>>::wrap(std::forward<Arg>(arg));
        }
        auto merge_with(Wrapper_arena&& other) & -> void {
            (Wrapper_arena<Ts>::merge_with(static_cast<Wrapper_arena<Ts>&&>(other)), ...);
        }
    };


    template <class T>
    class [[nodiscard]] Wrapper {
        T* m_pointer;
        explicit Wrapper(T* const pointer) noexcept
            : m_pointer { pointer } {}
        friend Wrapper_arena<T>;
    public:
        [[nodiscard]]
        auto operator*() const noexcept -> T& {
            APPLY_EXPLICIT_OBJECT_PARAMETER_HERE;
            return *m_pointer;
        }
        [[nodiscard]]
        auto operator->() const noexcept -> T* {
            APPLY_EXPLICIT_OBJECT_PARAMETER_HERE;
            return std::addressof(**this);
        }
    };

    template <class T>
    concept wrapper = instance_of<T, Wrapper>;

}


template <utl::hashable T>
struct std::hash<utl::Wrapper<T>> : hash<T> {
    [[nodiscard]] auto operator()(utl::Wrapper<T> const wrapper) const -> utl::Usize {
        return hash<T>::operator()(*wrapper);
    }
};

template <class T>
struct fmt::formatter<utl::Wrapper<T>> : formatter<T> {
    auto format(utl::Wrapper<T> const wrapper, auto& context) {
        return formatter<T>::format(*wrapper, context);
    }
};
