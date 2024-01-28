#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>

namespace utl {

    class [[nodiscard]] Source {
        std::filesystem::path m_file_path;
        std::string           m_file_content;
    public:
        using Arena   = utl::Wrapper_arena<Source>;
        using Wrapper = utl::Wrapper<Source>;

        // Create a source with the given path and content
        explicit Source(std::filesystem::path&&, std::string&&);

        enum class Read_error { does_not_exist, failed_to_open, failed_to_read };

        // Attempt to read a file with the given path
        static auto read(std::filesystem::path&&) -> std::expected<Source, Read_error>;

        [[nodiscard]] auto path() const noexcept -> std::filesystem::path const&;
        [[nodiscard]] auto string() const noexcept -> std::string_view;
    };

    struct Source_position {
        std::uint32_t line   = 1;
        std::uint32_t column = 1;

        auto advance_with(char) noexcept -> void;

        auto operator==(Source_position const&) const -> bool                  = default;
        auto operator<=>(Source_position const&) const -> std::strong_ordering = default;
    };

    struct Source_range {
        Source_position start;
        Source_position stop;

        // Deliberately non-aggregate.
        explicit constexpr Source_range(
            Source_position const start, Source_position const stop) noexcept
            : start { start }
            , stop { stop }
        {}

        [[nodiscard]] auto in(std::string_view string) const -> std::string_view;
        [[nodiscard]] auto up_to(Source_range other) const -> Source_range;
    };

} // namespace utl
