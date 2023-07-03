#include <libutl/common/utilities.hpp>
#include <libformat/format.hpp>


namespace libformat {
    struct [[nodiscard]] Newline;
    struct [[nodiscard]] Indentation;

    struct State {
        kieli::Format_configuration const& configuration;
        std::string&                       string;
        utl::Usize                         current_indentation {};

        auto newline() noexcept -> Newline;
        auto indent() noexcept -> Indentation;

        template <class... Args>
        auto format(std::format_string<Args...> const fmt, Args&&... args) -> void {
            std::format_to(std::back_inserter(string), fmt, std::forward<Args>(args)...);
        }

        auto format(cst::Expression                                         const&) -> void;
        auto format(cst::Pattern                                            const&) -> void;
        auto format(cst::Type                                               const&) -> void;
        auto format(cst::Qualified_name                                     const&) -> void;
        auto format(cst::Template_argument                                  const&) -> void;
        auto format(cst::Function_argument                                  const&) -> void;
        auto format(cst::Mutability                                         const&) -> void;
        auto format(tl::optional<cst::Template_arguments>                   const&) -> void;
        auto format(cst::Function_arguments                                 const&) -> void;
        auto format(cst::Class_reference                                    const&) -> void;
        auto format(cst::expression::Struct_initializer::Member_initializer const&) -> void;

        auto format_mutability_with_trailing_whitespace(tl::optional<cst::Mutability> const&) -> void;

        auto format(utl::wrapper auto const wrapper) -> void {
            format(*wrapper);
        }
        template <class T>
        auto format_separated(std::vector<T> const& vector, std::string_view const delimiter) -> void {
            if (vector.empty()) return;
            format(vector.front());
            std::for_each(vector.begin() + 1, vector.end(), [&](T const& element) {
                format("{}", delimiter);
                format(element);
            });
        }
        template <class T>
        auto format_comma_separated(std::vector<T> const& vector) -> void {
            format_separated(vector, ", ");
        }
    };

    struct Newline {
        utl::Usize indentation {};
    };
    struct Indentation {
        State& state;
        ~Indentation();
    };
}

template <>
struct std::formatter<libformat::Newline> : utl::formatting::Formatter_base {
    auto format(libformat::Newline const newline, auto& context) const {
        return std::format_to(context.out(), "\n{:{}}", "", newline.indentation);
    }
};
