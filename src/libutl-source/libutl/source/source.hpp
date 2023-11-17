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

        // Create a source with the given path and read the content from that file
        static auto read(std::filesystem::path&&) -> Source;

        [[nodiscard]] auto path() const noexcept -> std::filesystem::path const&;
        [[nodiscard]] auto string() const noexcept -> std::string_view;
    };

    struct Source_position {
        Usize line   = 1;
        Usize column = 1;

        auto advance_with(char) noexcept -> void;

        auto operator==(Source_position const&) const noexcept -> bool                  = default;
        auto operator<=>(Source_position const&) const noexcept -> std::strong_ordering = default;
    };

    struct [[nodiscard]] Source_view {
        Wrapper<Source>  source;
        std::string_view string;
        Source_position  start_position;
        Source_position  stop_position;

        explicit Source_view(
            Source::Wrapper  source,
            std::string_view string,
            Source_position  start,
            Source_position  stop) noexcept;

        // Dummy source view for mock purposes
        static auto dummy() -> Source_view;

        auto combine_with(Source_view const&) const noexcept -> Source_view;
    };
} // namespace utl
