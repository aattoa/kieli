#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

auto ki::fmt::newline(State const& state, std::size_t const lines) noexcept -> Newline
{
    return Newline {
        .indentation = state.indentation,
        .lines       = lines,
        .tab_size    = state.options.tab_size,
        .use_spaces  = state.options.use_spaces,
    };
}

void ki::fmt::format(State& state, cst::Expression_id const id)
{
    format(state, state.arena.expressions[id]);
}

void ki::fmt::format(State& state, cst::Pattern_id const id)
{
    format(state, state.arena.patterns[id]);
}

void ki::fmt::format(State& state, cst::Type_id const id)
{
    format(state, state.arena.types[id]);
}

void ki::fmt::format(State& state, cst::Wildcard const& wildcard)
{
    auto const [start, stop] = state.arena.ranges[wildcard.underscore_token];
    cpputil::always_assert(start.column < stop.column);
    format(state, "{:_^{}}", "", stop.column - start.column);
}

void ki::fmt::format(State& state, cst::Type_annotation const& annotation)
{
    format(state, ": ");
    format(state, annotation.type);
}

void ki::fmt::format(State& state, cst::Path const& path)
{
    if (auto const* type = std::get_if<cst::Type_id>(&path.root)) {
        format(state, *type);
    }
    for (auto const& segment : path.segments) {
        if (segment.leading_double_colon_token.has_value()) {
            format(state, "::");
        }
        format(state, "{}", state.pool.get(segment.name.id));
        format(state, segment.template_arguments);
    }
}

void ki::fmt::format(State& state, cst::Mutability const& mutability)
{
    auto const visitor = utl::Overload {
        [&](db::Mutability const concrete) {
            format(state, "{}", db::mutability_string(concrete));
        },
        [&](ki::cst::Parameterized_mutability const& parameterized) {
            format(state, "mut?{}", state.pool.get(parameterized.name.id));
        },
    };
    std::visit(visitor, mutability.variant);
}

void ki::fmt::format(State& state, cst::patt::Field const& field)
{
    format(state, "{}", state.pool.get(field.name.id));
    if (field.equals.has_value()) {
        format(state, " = ");
        format(state, field.equals.value().pattern);
    }
}

void ki::fmt::format(State& state, cst::Field_init const& initializer)
{
    format(state, "{}", state.pool.get(initializer.name.id));
    if (initializer.equals.has_value()) {
        format(state, " = ");
        format(state, initializer.equals.value().expression);
    }
}

void ki::fmt::format(State& state, cst::Field const& field)
{
    format(state, "{}", state.pool.get(field.name.id));
    format(state, field.type);
}

void ki::fmt::format_mutability_with_whitespace(
    State& state, std::optional<cst::Mutability> const& mutability)
{
    if (not mutability.has_value()) {
        return;
    }
    format(state, mutability.value());
    format(state, " ");
}

void ki::fmt::format(State& state, cst::Template_arguments const& arguments)
{
    format(state, "[");
    format_comma_separated(state, arguments.value.elements);
    format(state, "]");
}

void ki::fmt::format(State& state, cst::Template_parameter const& parameter)
{
    std::visit(
        utl::Overload {
            [&](cst::Template_type_parameter const& parameter) {
                format(state, "{}", state.pool.get(parameter.name.id));
                if (parameter.colon_token.has_value()) {
                    format(state, ": ");
                    format_separated(state, parameter.concepts.elements, " + ");
                }
                format(state, parameter.default_argument);
            },
            [&](cst::Template_value_parameter const& parameter) {
                format(state, "{}", state.pool.get(parameter.name.id));
                format(state, parameter.type_annotation);
                format(state, parameter.default_argument);
            },
            [&](cst::Template_mutability_parameter const& parameter) {
                format(state, "{}: mut", state.pool.get(parameter.name.id));
                format(state, parameter.default_argument);
            },
        },
        parameter.variant);
}

void ki::fmt::format(State& state, cst::Template_parameters const& parameters)
{
    format(state, "[");
    format_comma_separated(state, parameters.value.elements);
    format(state, "]");
}

void ki::fmt::format(State& state, cst::Function_arguments const& arguments)
{
    format(state, "(");
    format_comma_separated(state, arguments.value.elements);
    format(state, ")");
}

void ki::fmt::format(State& state, cst::Function_parameter const& parameter)
{
    format(state, parameter.pattern);
    format(state, parameter.type);
    format(state, parameter.default_argument);
}

void ki::fmt::format(State& state, cst::Function_parameters const& parameters)
{
    format(state, "(");
    format_comma_separated(state, parameters.value.elements);
    format(state, ")");
}
