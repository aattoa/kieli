#pragma once

#include "utl/utilities.hpp"


namespace utl {

    class [[nodiscard]] Bytestack {
        std::unique_ptr<std::byte[]> m_buffer;
        Usize                        m_length;
        std::byte*                   m_top_pointer;
    public:
        std::byte* pointer;

        explicit Bytestack(Usize const capacity) noexcept
            : m_buffer { std::make_unique_for_overwrite<std::byte[]>(capacity) }
        {
            // A member initializer list would mess up the initialization order
            m_length      = capacity;
            pointer       = m_buffer.get();
            m_top_pointer = pointer + capacity;
        }

        template <trivial T>
        auto push(T const x) noexcept -> void {
            if (pointer + sizeof x > m_top_pointer) [[unlikely]] {
                utl::abort("stack overflow");
            }
            std::memcpy(pointer, &x, sizeof x);
            pointer += sizeof x;
        }
        template <trivial T>
        auto pop() noexcept -> T {
            if (m_buffer.get() + sizeof(T) > pointer) [[unlikely]] {
                utl::abort("stack underflow");
            }
            T x;
            pointer -= sizeof x;
            std::memcpy(&x, pointer, sizeof x);
            return x;
        }
        template <trivial T>
        auto top() const noexcept -> T {
            if (m_buffer.get() + sizeof(T) > pointer) [[unlikely]] {
                utl::abort("stack underflow");
            }
            T x;
            std::memcpy(&x, pointer - sizeof x, sizeof x);
            return x;
        }

        [[nodiscard]] auto base()           noexcept -> std::byte      * { return m_buffer.get(); }
        [[nodiscard]] auto base()     const noexcept -> std::byte const* { return m_buffer.get(); }
        [[nodiscard]] auto capacity() const noexcept -> Usize            { return m_length; }
    };

}