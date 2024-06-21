#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

namespace utl {

    struct Source {
        std::string           content;
        std::filesystem::path path;
    };

    struct Source_id : utl::Vector_index<Source_id> {
        using Vector_index::Vector_index;
    };

    using Source_vector = utl::Index_vector<Source_id, Source>;

    struct Source_position {
        std::uint32_t line   = 1;
        std::uint32_t column = 1;

        // Advance this position with `character`.
        auto advance_with(char character) -> void;

        // Check that the position has non-zero components.
        [[nodiscard]] auto is_valid() const noexcept -> bool;

        auto operator==(Source_position const&) const -> bool = default;
        auto operator<=>(Source_position const&) const        = default;
    };

    struct Source_range {
        Source_position start;
        Source_position stop;

        // Construct a source range. Deliberately non-aggregate.
        explicit constexpr Source_range(Source_position start, Source_position stop) noexcept
            : start { start }
            , stop { stop }
        {}

        // Compute the substring of `string` corresponding to this source range.
        [[nodiscard]] auto in(std::string_view string) const -> std::string_view;

        // Create source range from `this` up to `other`.
        [[nodiscard]] auto up_to(Source_range other) const -> Source_range;

        // Source range with default constructed components for mock purposes.
        [[nodiscard]] static auto dummy() noexcept -> Source_range;
    };

    enum class Read_error { does_not_exist, failed_to_open, failed_to_read };

    auto read_file(std::filesystem::path const& path) -> std::expected<std::string, Read_error>;

} // namespace utl

template <>
struct std::formatter<utl::Source_position> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    auto format(utl::Source_position const position, auto& context) const
    {
        return std::format_to(context.out(), "{}:{}", position.line, position.column);
    }
};

template <>
struct std::formatter<utl::Source_range> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    auto format(utl::Source_range const range, auto& context) const
    {
        return std::format_to(context.out(), "({})-({})", range.start, range.stop);
    }
};
