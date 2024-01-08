#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>
#include <libformat/format.hpp>

namespace {
    struct Definition_format_visitor {
        libformat::State& state;

        auto operator()(cst::definition::Function const& function) const -> void
        {
            state.format("fn {}", function.signature.name);
            if (function.signature.template_parameters.has_value()) {
                state.format("[TODO]");
            }
            state.format("(TODO)");
            switch (state.configuration.function_body) {
            case kieli::Format_configuration::Function_body::leave_as_is:
            {
                if (function.optional_equals_sign_token.has_value()) {
                    state.format(" = ");
                    state.format(*function.body);
                }
                return;
            }
            case kieli::Format_configuration::Function_body::normalize_to_equals_sign:
            {
                if (auto const* const block
                    = std::get_if<cst::expression::Block>(&function.body->value))
                {
                    if (block->result_expression.has_value() && block->side_effects.empty()) {
                        state.format(" = ");
                        state.format(**block->result_expression);
                    }
                    else {
                        state.format(" ");
                        state.format(*function.body);
                    }
                }
                else {
                    state.format(" = ");
                    state.format(*function.body);
                }
                return;
            }
            case kieli::Format_configuration::Function_body::normalize_to_block:
            {
                if (std::holds_alternative<cst::expression::Block>(function.body->value)) {
                    state.format(" ");
                    state.format(*function.body);
                }
                else {
                    state.format(" {{ ");
                    state.format(*function.body);
                    state.format(" }}");
                }
                return;
            }
            default:
                cpputil::unreachable();
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

        auto operator()(cst::definition::Typeclass const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Implementation const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Instantiation const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Alias const&) -> void
        {
            cpputil::todo();
        }

        auto operator()(cst::definition::Namespace const&) -> void
        {
            cpputil::todo();
        }
    };

    auto format_node_impl(auto const& node, kieli::Format_configuration const& configuration)
        -> std::string
    {
        std::string      output_string;
        libformat::State state {
            .configuration       = configuration,
            .string              = output_string,
            .current_indentation = 0,
        };
        state.format(node);
        return output_string;
    }
} // namespace

auto kieli::format_module(
    cst::Module const& module, kieli::Format_configuration const& configuration) -> std::string
{
    std::string      output_string;
    libformat::State state {
        .configuration       = configuration,
        .string              = output_string,
        .current_indentation = 0,
    };
    for (cst::Definition const& definition : module.definitions) {
        std::visit(Definition_format_visitor { state }, definition.value);
    }
    output_string.push_back('\n');
    return output_string;
}

auto kieli::format_expression(
    cst::Expression const& expression, Format_configuration const& configuration) -> std::string
{
    return format_node_impl(expression, configuration);
}

auto kieli::format_pattern(cst::Pattern const& pattern, Format_configuration const& configuration)
    -> std::string
{
    return format_node_impl(pattern, configuration);
}

auto kieli::format_type(cst::Type const& type, Format_configuration const& configuration)
    -> std::string
{
    return format_node_impl(type, configuration);
}
