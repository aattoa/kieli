#pragma once

#include "utl/utilities.hpp"


namespace utl::dtl {
    template <class T, Usize page_size>
        requires (page_size != 0)
    class [[nodiscard]] Wrapper_arena_page {
        T* m_buffer;
        T* m_slot;
    public:
        Wrapper_arena_page() {
            m_slot = m_buffer = static_cast<T*>(::operator new(sizeof(T) * page_size, static_cast<std::align_val_t>(alignof(T))));
        }

        Wrapper_arena_page(Wrapper_arena_page&& other) noexcept
            : m_buffer { std::exchange(other.m_buffer, nullptr) }
            , m_slot   { std::exchange(other.m_slot, nullptr) } {}

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
            ::operator delete(m_buffer, sizeof(T) * page_size, static_cast<std::align_val_t>(alignof(T)));
        }

        template <class... Args> [[nodiscard]]
        auto unsafe_emplace_arena_element(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args&&...>) -> T*
        {
            assert(m_slot != m_buffer + page_size);
            std::construct_at(m_slot, std::forward<Args>(args)...);
            return m_slot++;

            // The m_slot increment has to be done after the construct_at in case
            // an exception is thrown, which would otherwise leave the slot with
            // an unconstructed object in it, which would lead to UB upon destruction.
        }
    };

    template <class T, Usize page_size>
    class [[nodiscard]] Wrapper_arena {
        using Page = Wrapper_arena_page<T, page_size>;
        std::vector<Page> m_pages;
        Usize             m_size = 0;
    public:
        explicit Wrapper_arena(Usize const initial_capacity) {
            Usize pages_needed = initial_capacity / page_size;
            if (initial_capacity % page_size != 0) ++pages_needed;
            m_pages.reserve(pages_needed);
            std::generate_n(std::back_inserter(m_pages), pages_needed, utl::make<Page>);
        }
        template <class... Args> [[nodiscard]]
        auto make_arena_element(Args&&... args) -> T* {
            // If all pages have been exhausted, append a fresh one.
            if (m_size++ == page_size * m_pages.size()) {
                m_pages.emplace_back();
            }
            return m_pages.back().unsafe_emplace_arena_element(std::forward<Args>(args)...);
        }
        [[nodiscard]]
        auto size() const noexcept -> Usize {
            return m_size;
        }
    };
}


namespace utl {

    inline constexpr Usize wrapper_arena_page_size                = 1024;
    inline constexpr Usize default_wrapper_arena_initial_capacity = wrapper_arena_page_size;

    template <class...>
    class [[nodiscard]] Wrapper_context;


    template <class T>
    class [[nodiscard]] Wrapper {
        T* m_pointer;
        explicit Wrapper(T* const pointer) noexcept
            : m_pointer { pointer } {}
    public:
        template <class... Args>
        static auto make(Args&&... args) -> Wrapper {
            assert(context_pointer != nullptr);
            return Wrapper { context_pointer->m_arena.make_arena_element(std::forward<Args>(args)...) };
        }
        [[nodiscard]]
        auto operator*(this Wrapper const self) noexcept -> T& {
            assert(context_pointer != nullptr);
            return *self.m_pointer;
        }
        [[nodiscard]]
        auto operator->(this Wrapper const self) noexcept -> T* {
            assert(context_pointer != nullptr);
            return self.m_pointer;
        }
    private:
        inline static thread_local constinit Wrapper_context<T>* context_pointer = nullptr;
        friend class Wrapper_context<T>;
    };


    template <class T>
    class [[nodiscard]] Wrapper_context<T> {
        dtl::Wrapper_arena<T, wrapper_arena_page_size> m_arena;
        bool                                           m_is_responsible = true;
    public:
        Wrapper_context(
            Usize                const initial_capacity = default_wrapper_arena_initial_capacity,
            std::source_location const caller = std::source_location::current())
            : m_arena { initial_capacity }
        {
            if (std::exchange(Wrapper<T>::context_pointer, this) != nullptr) {
                abort("Attempted to reinitialize a wrapper arena", caller);
            }
        }
        Wrapper_context(Wrapper_context&& other) noexcept
            : m_arena { std::move(other.m_arena) }
        {
            always_assert(std::exchange(other.m_is_responsible, false));
            Wrapper<T>::context_pointer = this;
        }
        ~Wrapper_context() {
            if (m_is_responsible) {
                Wrapper<T>::context_pointer = nullptr;
            }
        }
        [[nodiscard]]
        auto arena_size() const noexcept -> Usize {
            return m_arena.size();
        }
    private:
        friend class Wrapper<T>;
    };

    template <class... Ts>
    class [[nodiscard]] Wrapper_context : Wrapper_context<Ts>... {
    public:
        Wrapper_context(Usize const initial_capacity = default_wrapper_arena_initial_capacity)
            : Wrapper_context<Ts> { initial_capacity }... {}
        Wrapper_context(Wrapper_context<Ts>&&... children) noexcept
            : Wrapper_context<Ts> { std::move(children) }... {}
        template <one_of<Ts...> T> [[nodiscard]]
        auto arena_size() const noexcept -> Usize {
            return Wrapper_context<T>::arena_size();
        }
    };


    inline constexpr auto wrap = []<class X>(X&& x) {
        return Wrapper<std::decay_t<X>>::make(std::forward<X>(x));
    };

    template <class T>
    concept wrapper = instance_of<T, Wrapper>;

}



template <utl::hashable T>
struct std::hash<utl::Wrapper<T>> {
    [[nodiscard]] auto operator()(utl::Wrapper<T> const wrapper) const -> utl::Usize {
        return utl::hash(*wrapper);
    }
};

template <class T>
struct std::formatter<utl::Wrapper<T>> : std::formatter<T> {
    auto format(utl::Wrapper<T> const wrapper, std::format_context& context) {
        return std::formatter<T>::format(*wrapper, context);
    }
};