#ifndef KIELI_LIBUTL_DISJOINT_SET
#define KIELI_LIBUTL_DISJOINT_SET

#include <libutl/utilities.hpp>

namespace ki::utl {

    class Disjoint_set {
        std::vector<std::size_t> m_parents;
        std::vector<std::size_t> m_weights;
    public:
        Disjoint_set() = default;
        explicit Disjoint_set(std::size_t size);

        // Replace the set containing `x` and the set containing `y` with their union.
        void merge(std::size_t x, std::size_t y);

        // Add a new node to the set.
        [[nodiscard]] auto add() -> std::size_t;

        // Find the representative of `x`. Compresses the path as it is traversed.
        [[nodiscard]] auto find(std::size_t x) -> std::size_t;

        // Find the representative of `x`.
        [[nodiscard]] auto find_without_compressing(std::size_t x) const -> std::size_t;
    };

} // namespace ki::utl

#endif // KIELI_LIBUTL_DISJOINT_SET
