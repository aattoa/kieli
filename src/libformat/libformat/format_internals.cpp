#include <libutl/common/utilities.hpp>
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

auto libformat::State::format(cst::Qualified_name const& name) -> void
{
    if (name.root_qualifier.has_value()) {
        std::visit(
            utl::Overload {
                [&](cst::Root_qualifier::Global) { format("global::"); },
                [&](utl::Wrapper<cst::Type> const type) {
                    format(*type);
                    format("::");
                },

            },
            name.root_qualifier->value);
    }
    for (auto const& qualifier : name.middle_qualifiers.elements) {
        format("{}", qualifier.name);
        format(qualifier.template_arguments);
        format("::");
    }
    format("{}", name.primary_name);
}

auto libformat::State::format(cst::Template_argument const& argument) -> void
{
    std::visit(
        utl::Overload {
            [&](cst::Template_argument::Wildcard const&) { format("_"); },
            [&](auto const& other) { format(other); },
        },
        argument.value);
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
            [&](cst::Mutability::Parameterized const& parameterized) {
                format("mut?{}", parameterized.name);
            },
            [&](cst::Mutability::Concrete const& concrete) {
                format("{}", concrete.is_mutable ? "mut" : "immut");
            },
        },
        mutability.value);
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
    format(reference.name);
    format(reference.template_arguments);
}

auto libformat::State::format(
    cst::expression::Struct_initializer::Member_initializer const& initializer) -> void
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
