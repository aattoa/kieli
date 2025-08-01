#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

void ki::fmt::newline(Context& ctx, std::size_t lines)
{
    auto write_n = [&](std::size_t n, char c) {
        for (std::size_t i = 0; i != n; ++i) {
            ctx.stream.put(c);
        }
    };

    write_n(lines, '\n');
    if (ctx.options.use_spaces) {
        write_n(ctx.indentation * ctx.options.tab_size, ' ');
    }
    else {
        write_n(ctx.indentation, '\t');
    }
}

void ki::fmt::indent(Context& ctx)
{
    auto const take = [](bool& ref) { return std::exchange(ref, false); };
    newline(ctx, take(ctx.is_first_definition) ? 0 : take(ctx.did_open_block) ? 1 : 2);
}

void ki::fmt::format(Context& ctx, cst::Expression_id const id)
{
    format(ctx, ctx.arena.expressions[id]);
}

void ki::fmt::format(Context& ctx, cst::Pattern_id const id)
{
    format(ctx, ctx.arena.patterns[id]);
}

void ki::fmt::format(Context& ctx, cst::Type_id const id)
{
    format(ctx, ctx.arena.types[id]);
}

void ki::fmt::format(Context& ctx, cst::Wildcard const& wildcard)
{
    auto [start, stop] = wildcard.underscore_token;
    cpputil::always_assert(start.column < stop.column);
    std::print(ctx.stream, "{:_^{}}", "", stop.column - start.column);
}

void ki::fmt::format(Context& ctx, cst::Type_annotation const& annotation)
{
    std::print(ctx.stream, ": ");
    format(ctx, annotation.type);
}

void ki::fmt::format(Context& ctx, cst::Path const& path)
{
    if (auto const* type = std::get_if<cst::Type_id>(&path.root)) {
        format(ctx, *type);
    }
    for (auto const& segment : path.segments) {
        if (segment.leading_double_colon_token.has_value()) {
            std::print(ctx.stream, "::");
        }
        std::print(ctx.stream, "{}", ctx.db.string_pool.get(segment.name.id));
        format(ctx, segment.template_arguments);
    }
}

void ki::fmt::format(Context& ctx, cst::Mutability const& mutability)
{
    auto const visitor = utl::Overload {
        [&](db::Mutability const concrete) {
            std::print(ctx.stream, "{}", db::mutability_string(concrete));
        },
        [&](ki::cst::Parameterized_mutability const& parameterized) {
            std::print(ctx.stream, "mut?{}", ctx.db.string_pool.get(parameterized.name.id));
        },
    };
    std::visit(visitor, mutability.variant);
}

void ki::fmt::format(Context& ctx, cst::patt::Field const& field)
{
    std::print(ctx.stream, "{}", ctx.db.string_pool.get(field.name.id));
    if (field.equals.has_value()) {
        std::print(ctx.stream, " = ");
        format(ctx, field.equals.value().pattern);
    }
}

void ki::fmt::format(Context& ctx, cst::Field_init const& initializer)
{
    std::print(ctx.stream, "{}", ctx.db.string_pool.get(initializer.name.id));
    if (initializer.equals.has_value()) {
        std::print(ctx.stream, " = ");
        format(ctx, initializer.equals.value().expression);
    }
}

void ki::fmt::format(Context& ctx, cst::Field const& field)
{
    std::print(ctx.stream, "{}", ctx.db.string_pool.get(field.name.id));
    format(ctx, field.type);
}

void ki::fmt::format_mutability_with_whitespace(
    Context& ctx, std::optional<cst::Mutability> const& mutability)
{
    if (not mutability.has_value()) {
        return;
    }
    format(ctx, mutability.value());
    std::print(ctx.stream, " ");
}

void ki::fmt::format(Context& ctx, cst::Template_arguments const& arguments)
{
    std::print(ctx.stream, "[");
    format_comma_separated(ctx, arguments.value.elements);
    std::print(ctx.stream, "]");
}

void ki::fmt::format(Context& ctx, cst::Template_parameter const& parameter)
{
    std::visit(
        utl::Overload {
            [&](cst::Template_type_parameter const& parameter) {
                std::print(ctx.stream, "{}", ctx.db.string_pool.get(parameter.name.id));
                if (parameter.colon_token.has_value()) {
                    std::print(ctx.stream, ": ");
                    format_separated(ctx, parameter.concepts.elements, " + ");
                }
                format(ctx, parameter.default_argument);
            },
            [&](cst::Template_value_parameter const& parameter) {
                std::print(ctx.stream, "{}", ctx.db.string_pool.get(parameter.name.id));
                format(ctx, parameter.type_annotation);
                format(ctx, parameter.default_argument);
            },
            [&](cst::Template_mutability_parameter const& parameter) {
                std::print(ctx.stream, "{}: mut", ctx.db.string_pool.get(parameter.name.id));
                format(ctx, parameter.default_argument);
            },
        },
        parameter.variant);
}

void ki::fmt::format(Context& ctx, cst::Template_parameters const& parameters)
{
    std::print(ctx.stream, "[");
    format_comma_separated(ctx, parameters.value.elements);
    std::print(ctx.stream, "]");
}

void ki::fmt::format(Context& ctx, cst::Function_arguments const& arguments)
{
    std::print(ctx.stream, "(");
    format_comma_separated(ctx, arguments.value.elements);
    std::print(ctx.stream, ")");
}

void ki::fmt::format(Context& ctx, cst::Function_parameter const& parameter)
{
    format(ctx, parameter.pattern);
    format(ctx, parameter.type);
    format(ctx, parameter.default_argument);
}

void ki::fmt::format(Context& ctx, cst::Function_parameters const& parameters)
{
    std::print(ctx.stream, "(");
    format_comma_separated(ctx, parameters.value.elements);
    std::print(ctx.stream, ")");
}
