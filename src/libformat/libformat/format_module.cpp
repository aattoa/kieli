#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>
#include <libformat/format.hpp>

using namespace libformat;

// TODO: collapse string literals, expand integer literals, insert digit separators

namespace {
    struct Definition_format_visitor {
        State& state;

        auto operator()(cst::definition::Function const& function) const -> void
        {
            state.format("fn {}", function.signature.name);
            state.format(function.signature.template_parameters);
            state.format(function.signature.function_parameters.value);
            state.format(function.signature.return_type);
            switch (state.config.function_body) {
            case kieli::Format_function_body::leave_as_is:
            {
                if (function.optional_equals_sign_token.has_value()) {
                    state.format(" = ");
                    state.format(function.body);
                }
                else {
                    state.format(" ");
                    state.format(function.body);
                }
                return;
            }
            case kieli::Format_function_body::normalize_to_equals_sign:
            {
                if (auto const* const block
                    = std::get_if<cst::expression::Block>(&function.body->variant)) {
                    if (block->result_expression.has_value() && block->side_effects.empty()) {
                        state.format(" = ");
                        state.format(block->result_expression.value());
                    }
                    else {
                        state.format(" ");
                        state.format(function.body);
                    }
                }
                else {
                    state.format(" = ");
                    state.format(function.body);
                }
                return;
            }
            case kieli::Format_function_body::normalize_to_block:
            {
                if (std::holds_alternative<cst::expression::Block>(function.body->variant)) {
                    state.format(" ");
                    state.format(function.body);
                }
                else {
                    state.format(" {{ ");
                    state.format(function.body);
                    state.format(" }}");
                }
                return;
            }
            default: cpputil::unreachable();
            }
        }

        auto operator()(cst::definition::Struct const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Enum const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Concept const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Implementation const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Alias const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Submodule const&) -> void
        {
            cpputil::todo();
        }
    };
} // namespace

auto kieli::format_module(CST::Module const& module, kieli::Format_configuration const& config)
    -> std::string
{
    State state { .config = config };

    if (!module.definitions.empty()) {
        auto const format = [&state](cst::Definition const& definition) {
            std::visit(Definition_format_visitor { state }, definition.variant);
        };
        format(module.definitions.front());
        auto const newlines = std::views::repeat('\n', config.space_between_definitions + 1);
        for (auto it = module.definitions.begin() + 1; it != module.definitions.end(); ++it) {
            state.output.append_range(newlines);
            format(*it);
        }
    }

    state.output.push_back('\n');
    return std::move(state.output);
}

auto kieli::format_definition(cst::Definition const& definition, Format_configuration const& config)
    -> std::string
{
    State state { .config = config };
    std::visit(Definition_format_visitor { state }, definition.variant);
    return std::move(state.output);
}

auto kieli::format_expression(cst::Expression const& expression, Format_configuration const& config)
    -> std::string
{
    State state { .config = config };
    state.format(expression);
    return std::move(state.output);
}

auto kieli::format_pattern(cst::Pattern const& pattern, Format_configuration const& config)
    -> std::string
{
    State state { .config = config };
    state.format(pattern);
    return std::move(state.output);
}

auto kieli::format_type(cst::Type const& type, Format_configuration const& config) -> std::string
{
    State state { .config = config };
    state.format(type);
    return std::move(state.output);
}
