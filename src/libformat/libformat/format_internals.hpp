#include <libutl/common/utilities.hpp>
#include <libformat/format.hpp>


namespace libformat {
    struct [[nodiscard]] Newline;

    struct State {
        kieli::Format_configuration const& configuration;
        std::string&                       string;
        utl::Usize                         current_indentation {};

        auto newline() noexcept -> Newline;

        template <class... Args>
        auto format(std::format_string<Args...> const fmt, Args&&... args) -> void {
            std::format_to(std::back_inserter(string), fmt, std::forward<Args>(args)...);
        }
    };

    struct Newline {
        utl::Usize indentation {};
    };

    auto format_expression(cst::Expression const&, State&) -> void;
    auto format_pattern   (cst::Pattern    const&, State&) -> void;
    auto format_type      (cst::Type       const&, State&) -> void;
}

template <>
struct std::formatter<libformat::Newline> : utl::formatting::Formatter_base {
    auto format(libformat::Newline const newline, auto& context) {
        return std::format_to(context.out(), "{:>{}}", "", newline.indentation);
    }
};
