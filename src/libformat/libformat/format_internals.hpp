#pragma once

#include <libutl/utilities.hpp>
#include <libformat/format.hpp>

namespace libformat {
    namespace cst = kieli::cst;

    struct State;

    struct [[nodiscard]] Newline {
        std::size_t indentation {};
        std::size_t lines {};
        std::size_t tab_size {};
        bool        use_spaces {};
    };

    struct [[nodiscard]] Indentation {
        State& state;
        ~Indentation();
    };

    struct State {
        std::string                 output;
        kieli::Format_configuration config;
        std::size_t                 indentation {};
        cst::Arena const&           arena;

        auto newline(std::size_t lines = 1) const noexcept -> Newline;
        auto indent() noexcept -> Indentation;
    };

    template <class T>
    concept formattable = requires(State& state, T const& object) {
        { format(state, object) } -> std::same_as<void>;
    };

    auto format(State&, cst::Definition const&) -> void;
    auto format(State&, cst::Expression const&) -> void;
    auto format(State&, cst::Expression_id) -> void;
    auto format(State&, cst::Pattern const&) -> void;
    auto format(State&, cst::Pattern_id) -> void;
    auto format(State&, cst::Type const&) -> void;
    auto format(State&, cst::Type_id) -> void;
    auto format(State&, cst::Type_annotation const&) -> void;
    auto format(State&, cst::Wildcard const&) -> void;
    auto format(State&, cst::Path const&) -> void;
    auto format(State&, cst::Mutability const&) -> void;
    auto format(State&, cst::pattern::Field const&) -> void;
    auto format(State&, cst::Struct_field_initializer const&) -> void;
    auto format(State&, cst::definition::Field const&) -> void;
    auto format(State&, cst::Template_arguments const&) -> void;
    auto format(State&, cst::Template_parameter const&) -> void;
    auto format(State&, cst::Template_parameters const&) -> void;
    auto format(State&, cst::Function_arguments const&) -> void;
    auto format(State&, cst::Function_parameter const&) -> void;
    auto format(State&, cst::Function_parameters const&) -> void;

    template <class... Args>
    auto format(State& state, std::format_string<Args...> const fmt, Args&&... args) -> void
    {
        std::format_to(std::back_inserter(state.output), fmt, std::forward<Args>(args)...);
    }

    auto format_mutability_with_whitespace(
        State& state, std::optional<cst::Mutability> const& mutability) -> void;

    template <formattable... Ts>
    auto format(State& state, std::variant<Ts...> const& variant) -> void
    {
        std::visit([&](auto const& alternative) { format(state, alternative); }, variant);
    }

    template <formattable T>
    auto format(State& state, std::optional<T> const& optional) -> void
    {
        if (optional.has_value()) {
            format(state, optional.value());
        }
    }

    template <formattable T>
    auto format(State& state, cst::Default_argument<T> const& argument) -> void
    {
        format(state, " = ");
        format(state, argument.variant);
    }

    template <formattable T>
    auto format_separated(
        State& state, std::vector<T> const& vector, std::string_view const delimiter) -> void
    {
        if (vector.empty()) {
            return;
        }
        format(state, vector.front());
        std::for_each(vector.begin() + 1, vector.end(), [&](T const& element) {
            state.output.append(delimiter);
            format(state, element);
        });
    }

    template <formattable T>
    auto format_comma_separated(State& state, std::vector<T> const& vector) -> void
    {
        format_separated(state, vector, ", ");
    }
} // namespace libformat

template <>
struct std::formatter<libformat::Newline> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(libformat::Newline const& newline, auto& context)
    {
        auto out = context.out();
        utl::times(newline.lines, [&] { *out++ = '\n'; });

        if (newline.use_spaces) {
            utl::times(newline.indentation * newline.tab_size, [&] { *out++ = ' '; });
        }
        else {
            utl::times(newline.indentation, [&] { *out++ = '\t'; });
        }

        return out;
    }
};
