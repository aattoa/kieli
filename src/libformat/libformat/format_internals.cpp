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

auto libformat::format(State& state, cst::Expression_id const id) -> void
{
    format(state, state.arena.expressions[id]);
}

auto libformat::format(State& state, cst::Pattern_id const id) -> void
{
    format(state, state.arena.patterns[id]);
}

auto libformat::format(State& state, cst::Type_id const id) -> void
{
    format(state, state.arena.types[id]);
}

auto libformat::format(State& state, cst::Wildcard const& wildcard) -> void
{
    auto const [start, stop] = state.arena.tokens[wildcard.underscore_token].range;
    cpputil::always_assert(start.column < stop.column);
    format(state, "{:_^{}}", "", stop.column - start.column);
}

auto libformat::format(State& state, cst::Type_annotation const& annotation) -> void
{
    format(state, ": ");
    format(state, annotation.type);
}

auto libformat::format(State& state, cst::Path const& path) -> void
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

auto libformat::format(State& state, cst::Mutability const& mutability) -> void
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

auto libformat::format(State& state, cst::pattern::Field const& field) -> void
{
    format(state, "{}", field.name);
    if (field.field_pattern.has_value()) {
        format(state, " = ");
        format(state, field.field_pattern.value().pattern);
    }
}

auto libformat::format(State& state, cst::Struct_field_initializer const& initializer) -> void
{
    format(state, "{}", initializer.name);
    if (initializer.equals.has_value()) {
        format(state, " = ");
        format(state, initializer.equals.value().expression);
    }
}

auto libformat::format(State& state, cst::definition::Field const& field) -> void
{
    format(state, "{}", field.name);
    format(state, field.type);
}

auto libformat::format_mutability_with_whitespace(
    State& state, std::optional<cst::Mutability> const& mutability) -> void
{
    if (!mutability.has_value()) {
        return;
    }
    format(state, mutability.value());
    format(state, " ");
}

auto libformat::format(State& state, cst::Template_arguments const& arguments) -> void
{
    format(state, "[");
    format_comma_separated(state, arguments.value.elements);
    format(state, "]");
}

auto libformat::format(State& state, cst::Template_parameter const& parameter) -> void
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

auto libformat::format(State& state, cst::Template_parameters const& parameters) -> void
{
    format(state, "[");
    format_comma_separated(state, parameters.value.elements);
    format(state, "]");
}

auto libformat::format(State& state, cst::Function_arguments const& arguments) -> void
{
    format(state, "(");
    format_comma_separated(state, arguments.value.elements);
    format(state, ")");
}

auto libformat::format(State& state, cst::Function_parameter const& parameter) -> void
{
    format(state, parameter.pattern);
    format(state, parameter.type);
    format(state, parameter.default_argument);
}

auto libformat::format(State& state, cst::Function_parameters const& parameters) -> void
{
    format(state, "(");
    format_comma_separated(state, parameters.value.elements);
    format(state, ")");
}
