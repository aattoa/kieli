#pragma once

#include <libutl/utilities.hpp>
#include <libformat/format.hpp>
#include <libcompiler/cst/cst.hpp>

namespace libformat {
    namespace cst = kieli::cst;

    struct State;

    struct Newline {
        std::size_t indentation {};
        std::size_t lines {};
        std::size_t tab_size {};
        bool        use_spaces {};
    };

    struct Indentation {
        State& state;
        ~Indentation();
    };

    struct State {
        cst::Arena const&            arena;
        kieli::Format_options const& options;
        std::string&                 output;
        std::size_t                  indentation {};

        [[nodiscard]] auto newline(std::size_t lines = 1) const noexcept -> Newline;
        [[nodiscard]] auto indent() noexcept -> Indentation;
    };

    template <typename T>
    concept formattable = requires(State& state, T const& object) {
        { format(state, object) } -> std::same_as<void>;
    };

    void format(State&, cst::Definition const&);
    void format(State&, cst::Expression const&);
    void format(State&, cst::Expression_id);
    void format(State&, cst::Pattern const&);
    void format(State&, cst::Pattern_id);
    void format(State&, cst::Type const&);
    void format(State&, cst::Type_id);
    void format(State&, cst::Type_annotation const&);
    void format(State&, cst::Wildcard const&);
    void format(State&, cst::Path const&);
    void format(State&, cst::Mutability const&);
    void format(State&, cst::pattern::Field const&);
    void format(State&, cst::Struct_field_initializer const&);
    void format(State&, cst::definition::Field const&);
    void format(State&, cst::Template_arguments const&);
    void format(State&, cst::Template_parameter const&);
    void format(State&, cst::Template_parameters const&);
    void format(State&, cst::Function_arguments const&);
    void format(State&, cst::Function_parameter const&);
    void format(State&, cst::Function_parameters const&);

    template <typename... Args>
    void format(State& state, std::format_string<Args...> const fmt, Args&&... args)
    {
        std::format_to(std::back_inserter(state.output), fmt, std::forward<Args>(args)...);
    }

    void format_mutability_with_whitespace(
        State& state, std::optional<cst::Mutability> const& mutability);

    template <formattable... Ts>
    void format(State& state, std::variant<Ts...> const& variant)
    {
        std::visit([&](auto const& alternative) { format(state, alternative); }, variant);
    }

    template <formattable T>
    void format(State& state, std::optional<T> const& optional)
    {
        if (optional.has_value()) {
            format(state, optional.value());
        }
    }

    template <formattable T>
    void format(State& state, cst::Default_argument<T> const& argument)
    {
        format(state, " = ");
        format(state, argument.variant);
    }

    template <formattable T>
    void format_separated(
        State& state, std::vector<T> const& vector, std::string_view const delimiter)
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
    void format_comma_separated(State& state, std::vector<T> const& vector)
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
