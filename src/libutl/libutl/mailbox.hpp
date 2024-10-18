#pragma once

#include <libutl/utilities.hpp>

namespace utl {

    // Thread-safe queue.
    template <typename T>
    class Mailbox {
        mutable std::mutex m_mutex;
        std::queue<T>      m_queue;
    public:
        Mailbox() = default;

        [[nodiscard]] auto is_empty() const -> bool
        {
            std::scoped_lock _(m_mutex);
            return m_queue.empty();
        }

        [[nodiscard]] auto pop() -> std::optional<T>
        {
            std::scoped_lock _(m_mutex);
            if (not m_queue.empty()) {
                T mail = std::move(m_queue.front());
                m_queue.pop();
                return mail;
            }
            return std::nullopt;
        }

        template <typename... Args>
        void push(Args&&... args)
            requires std::is_constructible_v<T, Args...>
        {
            std::scoped_lock _(m_mutex);
            m_queue.emplace(std::forward<Args>(args)...);
        }
    };

} // namespace utl
