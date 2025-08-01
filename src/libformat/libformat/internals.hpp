#ifndef KIELI_LIBFORMAT_INTERNALS
#define KIELI_LIBFORMAT_INTERNALS

#include <libutl/utilities.hpp>
#include <libformat/format.hpp>
#include <libcompiler/cst/cst.hpp>

namespace ki::fmt {

    void newline(Context& ctx, std::size_t lines = 1);

    void indent(Context& ctx);

    void indent(Context& ctx, std::invocable auto block)
    {
        ++ctx.indentation;
        std::invoke(std::move(block));
        --ctx.indentation;
    }

    void format(Context& ctx, cst::Type_annotation const& annotation);
    void format(Context& ctx, cst::Wildcard const& wildcard);
    void format(Context& ctx, cst::Path const& path);
    void format(Context& ctx, cst::Mutability const& mutability);
    void format(Context& ctx, cst::patt::Field const& field);
    void format(Context& ctx, cst::Field_init const& init);
    void format(Context& ctx, cst::Field const& field);
    void format(Context& ctx, cst::Template_arguments const& arguments);
    void format(Context& ctx, cst::Template_parameter const& parameter);
    void format(Context& ctx, cst::Template_parameters const& parameters);
    void format(Context& ctx, cst::Function_arguments const& arguments);
    void format(Context& ctx, cst::Function_parameter const& parameter);
    void format(Context& ctx, cst::Function_parameters const& parameters);

    void format_mutability_with_whitespace(
        Context& ctx, std::optional<cst::Mutability> const& mutability);

    template <typename... Ts>
    void format(Context& ctx, std::variant<Ts...> const& variant)
    {
        std::visit([&](auto const& alternative) { format(ctx, alternative); }, variant);
    }

    template <typename T>
    void format(Context& ctx, std::optional<T> const& optional)
    {
        if (optional.has_value()) {
            format(ctx, optional.value());
        }
    }

    template <typename T>
    void format(Context& ctx, cst::Default_argument<T> const& argument)
    {
        std::print(ctx.stream, " = ");
        format(ctx, argument.variant);
    }

    template <typename T>
    void format_separated(Context& ctx, std::vector<T> const& vector, std::string_view delimiter)
    {
        if (vector.empty()) {
            return;
        }
        format(ctx, vector.front());
        std::for_each(vector.begin() + 1, vector.end(), [&](T const& element) {
            std::print(ctx.stream, "{}", delimiter);
            format(ctx, element);
        });
    }

    template <typename T>
    void format_comma_separated(Context& ctx, std::vector<T> const& vector)
    {
        format_separated(ctx, vector, ", ");
    }

} // namespace ki::fmt

#endif // KIELI_LIBFORMAT_INTERNALS
