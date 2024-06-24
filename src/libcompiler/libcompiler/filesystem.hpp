#pragma once

#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

namespace kieli {

    // In-memory representation of a file. Might not exist on the real filesystem.
    struct Source {
        std::string           content;
        std::filesystem::path path;
    };

    // Identifies a `Source`.
    struct Source_id : utl::Vector_index<Source_id> {
        using Vector_index::Vector_index;
    };

    // Vector of `Source`, indexed by `Source_id`.
    struct Source_vector : utl::Index_vector<Source_id, Source> {};

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
    struct Position {
        std::uint32_t line {};
        std::uint32_t column {};

        template <std::integral T>
        [[nodiscard]] static constexpr auto make(T const line, T const column) -> Position
        {
            return { utl::safe_cast<std::uint32_t>(line), utl::safe_cast<std::uint32_t>(column) };
        }

        // Advance this position with `character`.
        auto advance_with(char character) noexcept -> void;

        auto operator==(Position const&) const -> bool                  = default;
        auto operator<=>(Position const&) const -> std::strong_ordering = default;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
    struct Range {
        Position start;
        Position stop; // Exclusive position

        auto operator==(Range const&) const -> bool = default;
    };

    // Replace `range` in `text` with `new_text`.
    auto edit_text(std::string& text, Range range, std::string_view new_text) -> void;

    // Find the substring of `string` corresponding to `range`.
    [[nodiscard]] auto text_range(std::string_view string, Range range) -> std::string_view;

    // If `sources` contains a `Source` with `path`, return its `Source_id`.
    [[nodiscard]] auto find_source(std::filesystem::path const& path, Source_vector const& sources)
        -> std::optional<Source_id>;

    // Describes a file read failure.
    enum class Read_failure { does_not_exist, failed_to_open, failed_to_read };

    // Attempt to create a `Source` by reading the file at `path`.
    [[nodiscard]] auto read_source(std::filesystem::path path)
        -> std::expected<Source, Read_failure>;

    // Attempt to create a `Source` by reading the file at `path`, and add it to `sources`.
    [[nodiscard]] auto read_source(std::filesystem::path path, Source_vector& sources)
        -> std::expected<Source_id, Read_failure>;

} // namespace kieli

template <>
struct std::formatter<kieli::Position> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(kieli::Position const& position, auto& context)
    {
        return std::format_to(context.out(), "{}:{}", position.line, position.column);
    }
};

template <>
struct std::formatter<kieli::Range> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(kieli::Range const& range, auto& context)
    {
        return std::format_to(context.out(), "({}-{})", range.start, range.stop);
    }
};
