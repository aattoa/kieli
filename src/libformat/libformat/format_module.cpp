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
        format(state, definitions.front());
        for (auto it = definitions.begin() + 1; it != definitions.end(); ++it) {
            format(state, "{}", state.newline(state.options.empty_lines_between_definitions + 1));
            format(state, *it);
        }
    }

    auto format_function_signature(State& state, cst::Function_signature const& signature) -> void
    {
        format(state, "fn {}", signature.name);
        format(state, signature.template_parameters);
        format(state, signature.function_parameters);
        format(state, signature.return_type);
    }

    auto format_type_signature(State& state, cst::Type_signature const& signature) -> void
    {
        format(state, "alias {}", signature.name);
        format(state, signature.template_parameters);
        if (signature.concepts_colon_token.has_value()) {
            format(state, ": ");
            format_separated(state, signature.concepts.elements, " + ");
        }
    }

    auto format_constructor(State& state, cst::definition::Constructor_body const& body) -> void
    {
        auto const visitor = utl::Overload {
            [&](cst::definition::Struct_constructor const& constructor) {
                format(state, " {{ ");
                format_comma_separated(state, constructor.fields.value.elements);
                format(state, " }}");
            },
            [&](cst::definition::Tuple_constructor const& constructor) {
                format(state, "(");
                format_comma_separated(state, constructor.types.value.elements);
                format(state, ")");
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
            cst::Expression const& body = state.arena.expressions[function.body];

            switch (state.options.function_body) {
            case kieli::Format_function_body::leave_as_is:
            {
                if (function.optional_equals_sign_token.has_value()) {
                    format(state, " = ");
                    format(state, body);
                }
                else {
                    format(state, " ");
                    format(state, body);
                }
                return;
            }
            case kieli::Format_function_body::normalize_to_equals_sign:
            {
                if (auto const* const block = std::get_if<cst::expression::Block>(&body.variant)) {
                    if (block->result_expression.has_value() && block->side_effects.empty()) {
                        format(state, " = ");
                        format(state, block->result_expression.value());
                    }
                    else {
                        format(state, " ");
                        format(state, function.body);
                    }
                }
                else {
                    format(state, " = ");
                    format(state, function.body);
                }
                return;
            }
            case kieli::Format_function_body::normalize_to_block:
            {
                if (std::holds_alternative<cst::expression::Block>(body.variant)) {
                    format(state, " ");
                    format(state, function.body);
                }
                else {
                    format(state, " {{ ");
                    format(state, function.body);
                    format(state, " }}");
                }
                return;
            }
            default: cpputil::unreachable();
            }
        }

        auto operator()(cst::definition::Struct const& structure) -> void
        {
            format(state, "struct {}", structure.name);
            format(state, structure.template_parameters);
            format_constructor(state, structure.body);
        }

        auto operator()(cst::definition::Enum const& enumeration) -> void
        {
            format(state, "enum {}", enumeration.name);
            format(state, enumeration.template_parameters);
            format(state, " = ");

            auto const& constructors = enumeration.constructors.elements;

            cpputil::always_assert(!constructors.empty());
            format(state, "{}", constructors.front().name);
            format_constructor(state, constructors.front().body);

            auto const _ = state.indent();
            for (auto it = constructors.begin() + 1; it != constructors.end(); ++it) {
                format(state, "{}| {}", state.newline(), it->name);
                format_constructor(state, it->body);
            }
        }

        auto operator()(cst::definition::Concept const& concept_) -> void
        {
            format(state, "concept {}", concept_.name);
            format(state, concept_.template_parameters);
            format(state, " {{");
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
                    format(state, "{}", state.newline());
                    std::visit(visitor, requirement);
                }
            }
            format(state, "{}}}", state.newline());
        }

        auto operator()(cst::definition::Impl const& implementation) -> void
        {
            format(state, "impl");
            format(state, implementation.template_parameters);
            format(state, " ");
            format(state, implementation.self_type);
            format(state, " {{");
            {
                auto const _ = state.indent();
                format(state, "{}", state.newline());
                format_definitions(state, implementation.definitions.value);
            }
            format(state, "{}}}", state.newline());
        }

        auto operator()(cst::definition::Alias const& alias) -> void
        {
            format(state, "alias {}", alias.name);
            format(state, alias.template_parameters);
            format(state, " = ");
            format(state, alias.type);
        }

        auto operator()(cst::definition::Submodule const& module) -> void
        {
            format(state, "module {}", module.name);
            format(state, module.template_parameters);
            format(state, " {{");
            {
                auto const _ = state.indent();
                format(state, "{}", state.newline());
                format_definitions(state, module.definitions.value);
            }
            format(state, "{}}}", state.newline());
        }
    };
} // namespace

auto libformat::format(State& state, cst::Definition const& definition) -> void
{
    std::visit(Definition_format_visitor { state }, definition.variant);
}

auto kieli::format_module(CST::Module const& module, kieli::Format_options const& options)
    -> std::string
{
    std::string output;

    State state { module.arena, options, output };
    format_definitions(state, module.definitions);

    output.push_back('\n');
    return output;
}

auto kieli::format(
    cst::Arena const&      arena,
    Format_options const&  options,
    cst::Definition const& definition,
    std::string&           output) -> void
{
    State state { arena, options, output };
    libformat::format(state, definition);
}

auto kieli::format(
    cst::Arena const&      arena,
    Format_options const&  options,
    cst::Expression const& expression,
    std::string&           output) -> void
{
    State state { arena, options, output };
    libformat::format(state, expression);
}

auto kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Pattern const&   pattern,
    std::string&          output) -> void
{
    State state { arena, options, output };
    libformat::format(state, pattern);
}

auto kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Type const&      type,
    std::string&          output) -> void
{
    State state { arena, options, output };
    libformat::format(state, type);
}

auto kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Expression_id    expression_id,
    std::string&          output) -> void
{
    kieli::format(arena, options, arena.expressions[expression_id], output);
}

auto kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Pattern_id       pattern_id,
    std::string&          output) -> void
{
    kieli::format(arena, options, arena.patterns[pattern_id], output);
}

auto kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Type_id          type_id,
    std::string&          output) -> void
{
    kieli::format(arena, options, arena.types[type_id], output);
}
