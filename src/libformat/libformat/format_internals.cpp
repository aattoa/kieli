#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

auto libformat::State::newline() noexcept -> Newline
{
    return { .indentation = current_indentation };
}

auto libformat::State::indent() noexcept -> Indentation
{
    current_indentation += configuration.block_indentation;
    return { .state = *this };
}

libformat::Indentation::~Indentation()
{
    state.current_indentation -= state.configuration.block_indentation;
}

auto libformat::State::format(cst::Wildcard const& wildcard) -> void
{
    auto const [start, stop] = wildcard.range;
    cpputil::always_assert(start.column < stop.column);
    format("{:_^{}}", "", stop.column - start.column);
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

auto libformat::State::format(cst::Template_argument const& argument) -> void
{
    std::visit([&](auto const& other) { this->format(other); }, argument);
}

auto libformat::State::format(cst::Function_argument const& argument) -> void
{
    if (argument.name.has_value()) {
        format("{} = ", argument.name->name);
    }
    format(argument.expression);
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

auto libformat::State::format(std::optional<cst::Template_arguments> const& arguments) -> void
{
    if (!arguments.has_value()) {
        return;
    }
    format("[");
    format_comma_separated(arguments->value.elements);
    format("]");
}

auto libformat::State::format(cst::Function_arguments const& arguments) -> void
{
    format("(");
    format_comma_separated(arguments.value.elements);
    format(")");
}

auto libformat::State::format(cst::Class_reference const& reference) -> void
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

auto libformat::State::format_mutability_with_trailing_whitespace(
    std::optional<cst::Mutability> const& mutability) -> void
{
    if (!mutability.has_value()) {
        return;
    }
    format(*mutability);
    format(" ");
}
