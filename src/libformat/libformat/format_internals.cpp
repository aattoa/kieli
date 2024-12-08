#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

auto libformat::State::newline(std::size_t const lines) const noexcept -> Newline
{
    return {
        .indentation = indentation,
        .lines       = lines,
        .tab_size    = options.tab_size,
        .use_spaces  = options.use_spaces,
    };
}

auto libformat::State::indent() noexcept -> Indentation
{
    ++indentation;
    return { .state = *this };
}

libformat::Indentation::~Indentation()
{
    --state.indentation;
}

void libformat::format(State& state, cst::Expression_id const id)
{
    format(state, state.arena.expressions[id]);
}

void libformat::format(State& state, cst::Pattern_id const id)
{
    format(state, state.arena.patterns[id]);
}

void libformat::format(State& state, cst::Type_id const id)
{
    format(state, state.arena.types[id]);
}

void libformat::format(State& state, cst::Wildcard const& wildcard)
{
    auto const [start, stop] = state.arena.tokens[wildcard.underscore_token].range;
    cpputil::always_assert(start.column < stop.column);
    format(state, "{:_^{}}", "", stop.column - start.column);
}

void libformat::format(State& state, cst::Type_annotation const& annotation)
{
    format(state, ": ");
    format(state, annotation.type);
}

void libformat::format(State& state, cst::Path const& path)
{
    auto const visitor = utl::Overload {
        [&](std::monostate) {},
        [&](cst::Path_root_global const&) { format(state, "global"); },
        [&](cst::Type_id const type) { format(state, type); },
    };
    std::visit(visitor, path.root);
    for (auto const& segment : path.segments) {
        if (segment.leading_double_colon_token.has_value()) {
            format(state, "::");
        }
        format(state, "{}", segment.name);
        format(state, segment.template_arguments);
    }
}

void libformat::format(State& state, cst::Mutability const& mutability)
{
    auto const visitor = utl::Overload {
        [&](kieli::Mutability const concrete) {
            format(state, "{}", concrete); //
        },
        [&](kieli::cst::Parameterized_mutability const& parameterized) {
            format(state, "mut?{}", parameterized.name);
        },
    };
    return std::visit(visitor, mutability.variant);
}

void libformat::format(State& state, cst::pattern::Field const& field)
{
    format(state, "{}", field.name);
    if (field.equals.has_value()) {
        format(state, " = ");
        format(state, field.equals.value().pattern);
    }
}

void libformat::format(State& state, cst::Struct_field_initializer const& initializer)
{
    format(state, "{}", initializer.name);
    if (initializer.equals.has_value()) {
        format(state, " = ");
        format(state, initializer.equals.value().expression);
    }
}

void libformat::format(State& state, cst::definition::Field const& field)
{
    format(state, "{}", field.name);
    format(state, field.type);
}

void libformat::format_mutability_with_whitespace(
    State& state, std::optional<cst::Mutability> const& mutability)
{
    if (not mutability.has_value()) {
        return;
    }
    format(state, mutability.value());
    format(state, " ");
}

void libformat::format(State& state, cst::Template_arguments const& arguments)
{
    format(state, "[");
    format_comma_separated(state, arguments.value.elements);
    format(state, "]");
}

void libformat::format(State& state, cst::Template_parameter const& parameter)
{
    std::visit(
        utl::Overload {
            [&](cst::Template_type_parameter const& parameter) {
                format(state, "{}", parameter.name);
                if (parameter.colon_token.has_value()) {
                    format(state, ": ");
                    format_separated(state, parameter.concepts.elements, " + ");
                }
                format(state, parameter.default_argument);
            },
            [&](cst::Template_value_parameter const& parameter) {
                format(state, "{}", parameter.name);
                format(state, parameter.type_annotation);
                format(state, parameter.default_argument);
            },
            [&](cst::Template_mutability_parameter const& parameter) {
                format(state, "{}: mut", parameter.name);
                format(state, parameter.default_argument);
            },
        },
        parameter.variant);
}

void libformat::format(State& state, cst::Template_parameters const& parameters)
{
    format(state, "[");
    format_comma_separated(state, parameters.value.elements);
    format(state, "]");
}

void libformat::format(State& state, cst::Function_arguments const& arguments)
{
    format(state, "(");
    format_comma_separated(state, arguments.value.elements);
    format(state, ")");
}

void libformat::format(State& state, cst::Function_parameter const& parameter)
{
    format(state, parameter.pattern);
    format(state, parameter.type);
    format(state, parameter.default_argument);
}

void libformat::format(State& state, cst::Function_parameters const& parameters)
{
    format(state, "(");
    format_comma_separated(state, parameters.value.elements);
    format(state, ")");
}
