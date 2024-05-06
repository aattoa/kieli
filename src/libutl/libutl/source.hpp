#pragma once

#include <libutl/utilities.hpp>
#include <libutl/wrapper.hpp>

namespace utl {

    class [[nodiscard]] Source {
        std::filesystem::path m_file_path;
        std::string           m_file_content;
    public:
        using Arena   = utl::Wrapper_arena<Source>;
        using Wrapper = utl::Wrapper<Source>;

        enum class Read_error { does_not_exist, failed_to_open, failed_to_read };

        // Create a source with the given path and content
        explicit Source(std::filesystem::path&& path, std::string&& content);

        // Attempt to read a file with the given path
        [[nodiscard]] static auto read(std::filesystem::path) -> std::expected<Source, Read_error>;

        // File path accessor.
        [[nodiscard]] auto path() const noexcept -> std::filesystem::path const&;

        // File content accessor.
        [[nodiscard]] auto string() const noexcept -> std::string_view;
    };

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
        explicit constexpr Source_range(
            Source_position const start, Source_position const stop) noexcept
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
