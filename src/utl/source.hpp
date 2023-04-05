#pragma once

#include "utilities.hpp"


namespace utl {

    class [[nodiscard]] Source {
        std::string m_filename;
        std::string m_contents;
    public:
        struct Filename { std::string string; };

        explicit Source(Filename&&);
        explicit Source(Filename&&, std::string&& file_content);

        [[nodiscard]]
        auto filename() const noexcept -> std::string_view;
        [[nodiscard]]
        auto string() const noexcept -> std::string_view;
    };


    struct Source_position {
        Usize line   = 1;
        Usize column = 1;

        auto increment_with(char) noexcept -> void;

        auto operator==(Source_position const&) const noexcept -> bool = default;
        auto operator<=>(Source_position const&) const noexcept -> std::strong_ordering = default;
    };


    struct [[nodiscard]] Source_view {
        std::string_view string;
        Source_position  start_position;
        Source_position  stop_position;

        explicit constexpr Source_view(
            std::string_view const string,
            Source_position  const start,
            Source_position  const stop) noexcept
            : string         { string }
            , start_position { start  }
            , stop_position  { stop   }
        {
            assert(start_position <= stop_position);
        }

        // Dummy source view for mock purposes
        static constexpr auto dummy() -> Source_view {
            return Source_view { {}, {}, {} };
        }

        auto operator+(Source_view const&) const noexcept -> Source_view;
    };

}


DECLARE_FORMATTER_FOR(utl::Source_position);