#pragma once

#include <libutl/utilities.hpp>
#include <libformat/format.hpp>

namespace libformat {
    namespace cst = kieli::cst;

    struct [[nodiscard]] Newline;
    struct [[nodiscard]] Indentation;

    struct State {
        std::string                 output;
        kieli::Format_configuration config;
        std::size_t                 indentation {};

        auto newline(std::size_t count = 1) noexcept -> Newline;
        auto indent() noexcept -> Indentation;

        template <class... Args>
        auto format(std::format_string<Args...> const fmt, Args&&... args) -> void
        {
            std::format_to(std::back_inserter(output), fmt, std::forward<Args>(args)...);
        }

        auto format(cst::Definition const&) -> void;
        auto format(cst::Expression const&) -> void;
        auto format(cst::Pattern const&) -> void;
        auto format(cst::Type const&) -> void;
        auto format(cst::Type_annotation const&) -> void;
        auto format(cst::Wildcard const&) -> void;
        auto format(cst::Path const&) -> void;
        auto format(cst::Mutability const&) -> void;
        auto format(cst::Concept_reference const&) -> void;
        auto format(cst::pattern::Field const&) -> void;
        auto format(cst::expression::Struct_initializer::Field const&) -> void;
        auto format(cst::definition::Field const&) -> void;
        auto format(cst::Self_parameter const&) -> void;
        auto format(cst::Template_arguments const&) -> void;
        auto format(cst::Template_parameter const&) -> void;
        auto format(cst::Template_parameters const&) -> void;
        auto format(cst::Function_argument const&) -> void;
        auto format(cst::Function_arguments const&) -> void;
        auto format(cst::Function_parameter const&) -> void;
        auto format(cst::Function_parameters const&) -> void;

        auto format_mutability_with_whitespace(std::optional<cst::Mutability> const&) -> void;

        template <class T>
        auto format(utl::Wrapper<T> const wrapper) -> void
        {
            format(*wrapper);
        }

        template <class... Ts>
        auto format(std::variant<Ts...> const& variant) -> void
        {
            std::visit([this](auto const& alternative) { this->format(alternative); }, variant);
        }

        template <class T>
        auto format(std::optional<T> const& optional) -> void
        {
            if (optional.has_value()) {
                format(optional.value());
            }
        }

        template <class T>
        auto format(cst::Default_argument<T> const& argument) -> void
        {
            format(" = ");
            format(argument.variant);
        }

        template <class T>
        auto format_separated(std::vector<T> const& vector, std::string_view const delimiter)
            -> void
        {
            if (vector.empty()) {
                return;
            }
            format(vector.front());
            std::for_each(vector.begin() + 1, vector.end(), [&](T const& element) {
                output.append(delimiter);
                format(element);
            });
        }

        template <class T>
        auto format_comma_separated(std::vector<T> const& vector) -> void
        {
            format_separated(vector, ", ");
        }
    };

    struct Newline {
        std::size_t indentation {};
        std::size_t count {};
    };

    struct Indentation {
        State& state;
        ~Indentation();
    };
} // namespace libformat

template <>
struct std::formatter<libformat::Newline> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(libformat::Newline const newline, auto& context)
    {
        auto out = context.out();
        utl::times(newline.count, [&] { *out++ = '\n'; });
        utl::times(newline.indentation, [&] { *out++ = ' '; });
        return out;
    }
};
