#ifndef KIELI_LIBFORMAT_INTERNALS
#define KIELI_LIBFORMAT_INTERNALS

#include <libutl/utilities.hpp>
#include <libformat/format.hpp>
#include <libcompiler/cst/cst.hpp>

namespace ki::fmt {

    struct Newline {
        std::size_t indentation {};
        std::size_t lines {};
        std::size_t tab_size {};
        bool        use_spaces {};
    };

    struct State {
        utl::String_pool const& pool;
        cst::Arena const&       arena;
        Options const&          options;
        std::size_t             indentation {};
        std::string&            output;
    };

    auto newline(State const& state, std::size_t lines = 1) noexcept -> Newline;

    void indent(State& state, std::invocable auto block)
    {
        ++state.indentation;
        std::invoke(std::move(block));
        --state.indentation;
    }

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
    void format(State&, cst::patt::Field const&);
    void format(State&, cst::Field_init const&);
    void format(State&, cst::Field const&);
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

    template <typename... Ts>
    void format(State& state, std::variant<Ts...> const& variant)
    {
        std::visit([&](auto const& alternative) { format(state, alternative); }, variant);
    }

    template <typename T>
    void format(State& state, std::optional<T> const& optional)
    {
        if (optional.has_value()) {
            format(state, optional.value());
        }
    }

    template <typename T>
    void format(State& state, cst::Default_argument<T> const& argument)
    {
        format(state, " = ");
        format(state, argument.variant);
    }

    template <typename T>
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

    template <typename T>
    void format_comma_separated(State& state, std::vector<T> const& vector)
    {
        format_separated(state, vector, ", ");
    }

} // namespace ki::fmt

template <>
struct std::formatter<ki::fmt::Newline> {
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(ki::fmt::Newline const& newline, auto& ctx)
    {
        auto out = ctx.out();

        auto write_n = [&out](std::size_t n, char c) {
            for (std::size_t i = 0; i != n; ++i) {
                *out++ = c;
            }
        };

        write_n(newline.lines, '\n');
        if (newline.use_spaces) {
            write_n(newline.indentation * newline.tab_size, ' ');
        }
        else {
            write_n(newline.indentation, '\t');
        }

        return out;
    }
};

#endif // KIELI_LIBFORMAT_INTERNALS
