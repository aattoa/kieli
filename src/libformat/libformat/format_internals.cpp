#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

auto libformat::State::newline(std::size_t count) noexcept -> Newline
{
    return { .indentation = indentation, .count = count };
}

auto libformat::State::indent() noexcept -> Indentation
{
    indentation += config.block_indentation;
    return { .state = *this };
}

libformat::Indentation::~Indentation()
{
    state.indentation -= state.config.block_indentation;
}

auto libformat::State::format(cst::Wildcard const& wildcard) -> void
{
    auto const [start, stop] = wildcard.range;
    cpputil::always_assert(start.column < stop.column);
    format("{:_^{}}", "", stop.column - start.column);
}

auto libformat::State::format(cst::Type_annotation const& annotation) -> void
{
    format(": ");
    format(annotation.type);
}

auto libformat::State::format(cst::Path const& path) -> void
{
    if (path.root.has_value()) {
        std::visit(
            utl::Overload {
                [&](cst::Path_root_global) { format("global::"); },
                [&](utl::Wrapper<cst::Type> const type) {
                    format(*type);
                    format("::");
                },
            },
            path.root->variant);
    }
    for (auto const& segment : path.segments.elements) {
        format("{}", segment.name);
        format(segment.template_arguments);
        format("::");
    }
    format("{}", path.head);
}

auto libformat::State::format(cst::Mutability const& mutability) -> void
{
    return std::visit(
        utl::Overload {
            [&](cst::mutability::Parameterized const& parameterized) {
                format("mut?{}", parameterized.name);
            },
            [&](cst::mutability::Concrete const concrete) { format("{}", concrete); },
        },
        mutability.variant);
}

auto libformat::State::format(cst::Concept_reference const& reference) -> void
{
    format(reference.path);
    format(reference.template_arguments);
}

auto libformat::State::format(cst::pattern::Field const& field) -> void
{
    format("{}", field.name);
    if (field.field_pattern.has_value()) {
        format(" = ");
        format(field.field_pattern.value().pattern);
    }
}

auto libformat::State::format(cst::expression::Struct_initializer::Field const& initializer) -> void
{
    format("{} = ", initializer.name);
    format(initializer.expression);
}

auto libformat::State::format(cst::definition::Field const& field) -> void
{
    format("{}", field.name);
    format(field.type);
}

auto libformat::State::format_mutability_with_whitespace(
    std::optional<cst::Mutability> const& mutability) -> void
{
    if (!mutability.has_value()) {
        return;
    }
    format(*mutability);
    format(" ");
}

auto libformat::State::format(cst::Self_parameter const& parameter) -> void
{
    if (parameter.is_reference()) {
        format("&");
    }
    format_mutability_with_whitespace(parameter.mutability);
    format("self");
}

auto libformat::State::format(cst::Template_arguments const& arguments) -> void
{
    format("[");
    format_comma_separated(arguments.value.elements);
    format("]");
}

auto libformat::State::format(cst::Template_parameter const& parameter) -> void
{
    std::visit(
        utl::Overload {
            [&](cst::Template_type_parameter const& parameter) {
                format("{}", parameter.name);
                if (parameter.colon_token.has_value()) {
                    format(": ");
                    format_separated(parameter.concepts.elements, " + ");
                }
                format(parameter.default_argument);
            },
            [&](cst::Template_value_parameter const& parameter) {
                format("{}", parameter.name);
                format(parameter.type_annotation);
                format(parameter.default_argument);
            },
            [&](cst::Template_mutability_parameter const& parameter) {
                format("{}: mut", parameter.name);
                format(parameter.default_argument);
            },
        },
        parameter.variant);
}

auto libformat::State::format(cst::Template_parameters const& parameters) -> void
{
    format("[");
    format_comma_separated(parameters.value.elements);
    format("]");
}

auto libformat::State::format(cst::Function_argument const& argument) -> void
{
    if (argument.name.has_value()) {
        format("{} = ", argument.name.value().name);
    }
    format(argument.expression);
}

auto libformat::State::format(cst::Function_arguments const& arguments) -> void
{
    format("(");
    format_comma_separated(arguments.value.elements);
    format(")");
}

auto libformat::State::format(cst::Function_parameter const& parameter) -> void
{
    format(parameter.pattern);
    format(parameter.type);
    format(parameter.default_argument);
}

auto libformat::State::format(cst::Function_parameters const& parameters) -> void
{
    format("(");
    format(parameters.self_parameter);
    if (parameters.comma_token_after_self.has_value()) {
        format(", ");
    }
    format_comma_separated(parameters.normal_parameters.elements);
    format(")");
}
