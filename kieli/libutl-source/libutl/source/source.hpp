#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>


namespace utl {

    class [[nodiscard]] Source {
        std::filesystem::path m_file_path;
        std::string           m_file_content;
    public:
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

        auto operator==(Source_position const&) const noexcept -> bool = default;
        auto operator<=>(Source_position const&) const noexcept -> std::strong_ordering = default;
    };

    struct [[nodiscard]] Source_view {
        Wrapper<Source>  source;
        std::string_view string;
        Source_position  start_position;
        Source_position  stop_position;

        explicit Source_view(
            Wrapper<Source>  const source,
            std::string_view const string,
            Source_position  const start,
            Source_position  const stop) noexcept
            : source         { source }
            , string         { string }
            , start_position { start  }
            , stop_position  { stop   }
        {
            always_assert(start_position <= stop_position);
        }

        // Dummy source view for mock purposes
        static auto dummy() -> Source_view;

        auto operator+(Source_view const&) const noexcept -> Source_view;
    };

}


DECLARE_FORMATTER_FOR(utl::Source_position);
