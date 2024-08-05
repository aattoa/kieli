#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>
#include <libformat/format.hpp>

using namespace libformat;

// TODO: collapse string literals, expand integer literals, insert digit separators

namespace {
    auto format_definitions(State& state, std::vector<cst::Definition> const& definitions) -> void
    {
        if (definitions.empty()) {
            return;
        }
        state.format(definitions.front());
        for (auto it = definitions.begin() + 1; it != definitions.end(); ++it) {
            state.format("{}", state.newline(state.config.space_between_definitions + 1));
            state.format(*it);
        }
    }

    auto format_function_signature(State& state, cst::Function_signature const& signature) -> void
    {
        state.format("fn {}", signature.name);
        state.format(signature.template_parameters);
        state.format(signature.function_parameters.value);
        state.format(signature.return_type);
    }

    auto format_type_signature(State& state, cst::Type_signature const& signature) -> void
    {
        state.format("alias {}", signature.name);
        state.format(signature.template_parameters);
        if (signature.concepts_colon_token.has_value()) {
            state.format(": ");
            state.format_separated(signature.concepts.elements, " + ");
        }
    }

    auto format_constructor(State& state, cst::definition::Constructor_body const& body) -> void
    {
        auto const visitor = utl::Overload {
            [&](cst::definition::Struct_constructor const& constructor) {
                state.format(" {{ ");
                state.format_comma_separated(constructor.fields.value.elements);
                state.format(" }}");
            },
            [&](cst::definition::Tuple_constructor const& constructor) {
                state.format("(");
                state.format_comma_separated(constructor.types.value.elements);
                state.format(")");
            },
            [&](cst::definition::Unit_constructor const&) {},
        };
        std::visit(visitor, body);
    }

    struct Definition_format_visitor {
        State& state;

        auto operator()(cst::definition::Function const& function) const -> void
        {
            format_function_signature(state, function.signature);
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

        auto operator()(cst::definition::Struct const& structure) -> void
        {
            state.format("struct {}", structure.name);
            state.format(structure.template_parameters);
            format_constructor(state, structure.body);
        }

        auto operator()(cst::definition::Enum const& enumeration) -> void
        {
            state.format("enum {}", enumeration.name);
            state.format(enumeration.template_parameters);
            state.format(" = ");

            auto const& constructors = enumeration.constructors.elements;

            cpputil::always_assert(!constructors.empty());
            state.format("{}", constructors.front().name);
            format_constructor(state, constructors.front().body);

            auto const _ = state.indent();
            for (auto it = constructors.begin() + 1; it != constructors.end(); ++it) {
                state.format("{}| {}", state.newline(), it->name);
                format_constructor(state, it->body);
            }
        }

        auto operator()(cst::definition::Concept const& concept_) -> void
        {
            state.format("concept {}", concept_.name);
            state.format(concept_.template_parameters);
            state.format(" {{");
            {
                auto const _ = state.indent();
                for (auto const& requirement : concept_.requirements) {
                    auto const visitor = utl::Overload {
                        [&](cst::Function_signature const& signature) {
                            format_function_signature(state, signature);
                        },
                        [&](cst::Type_signature const& signature) {
                            format_type_signature(state, signature);
                        },
                    };
                    state.format("{}", state.newline());
                    std::visit(visitor, requirement);
                }
            }
            state.format("{}}}", state.newline());
        }

        auto operator()(cst::definition::Implementation const& implementation) -> void
        {
            state.format("impl");
            state.format(implementation.template_parameters);
            state.format(" ");
            state.format(implementation.self_type);
            state.format(" {{");
            {
                auto const _ = state.indent();
                state.format("{}", state.newline());
                format_definitions(state, implementation.definitions.value);
            }
            state.format("{}}}", state.newline());
        }

        auto operator()(cst::definition::Alias const& alias) -> void
        {
            state.format("alias {}", alias.name);
            state.format(alias.template_parameters);
            state.format(" = ");
            state.format(alias.type);
        }

        auto operator()(cst::definition::Submodule const& module) -> void
        {
            state.format("module {}", module.name);
            state.format(module.template_parameters);
            state.format(" {{");
            {
                auto const _ = state.indent();
                state.format("{}", state.newline());
                format_definitions(state, module.definitions.value);
            }
            state.format("{}}}", state.newline());
        }
    };
} // namespace

auto libformat::State::format(cst::Definition const& definition) -> void
{
    std::visit(Definition_format_visitor { *this }, definition.variant);
}

auto kieli::format_module(CST::Module const& module, kieli::Format_configuration const& config)
    -> std::string
{
    State state { .config = config };
    format_definitions(state, module.definitions);
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
