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
        Context&    ctx;
        std::string output;
    };

    auto newline(State const& state, std::size_t lines = 1) noexcept -> Newline;

    void indent(State& state, std::invocable auto block)
    {
        ++state.ctx.indentation;
        std::invoke(std::move(block));
        --state.ctx.indentation;
    }

    void format(State& state, cst::Function const& function);
    void format(State& state, cst::Struct const& structure);
    void format(State& state, cst::Enum const& enumeration);
    void format(State& state, cst::Alias const& alias);
    void format(State& state, cst::Concept const& concept_);
    void format(State& state, cst::Impl_begin const& impl);
    void format(State& state, cst::Submodule_begin const& submodule);
    void format(State& state, cst::Block_end const& block_end);
    void format(State& state, cst::Expression const& expression);
    void format(State& state, cst::Expression_id expression_id);
    void format(State& state, cst::Pattern const& pattern);
    void format(State& state, cst::Pattern_id pattern_id);
    void format(State& state, cst::Type const& type);
    void format(State& state, cst::Type_id type_id);
    void format(State& state, cst::Type_annotation const& annotation);
    void format(State& state, cst::Wildcard const& wildcard);
    void format(State& state, cst::Path const& path);
    void format(State& state, cst::Mutability const& mutability);
    void format(State& state, cst::patt::Field const& field);
    void format(State& state, cst::Field_init const& init);
    void format(State& state, cst::Field const& field);
    void format(State& state, cst::Template_arguments const& arguments);
    void format(State& state, cst::Template_parameter const& parameter);
    void format(State& state, cst::Template_parameters const& parameters);
    void format(State& state, cst::Function_arguments const& arguments);
    void format(State& state, cst::Function_parameter const& parameter);
    void format(State& state, cst::Function_parameters const& parameters);

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
