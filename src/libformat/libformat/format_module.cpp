#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>
#include <libformat/format.hpp>

using namespace libformat;

// TODO: collapse string literals, expand integer literals, insert digit separators

namespace {
    void format_definitions(State& state, std::vector<cst::Definition> const& definitions)
    {
        if (definitions.empty()) {
            return;
        }
        format(state, definitions.front());
        for (auto it = definitions.begin() + 1; it != definitions.end(); ++it) {
            format(state, "{}", newline(state, state.options.empty_lines_between_definitions + 1));
            format(state, *it);
        }
    }

    void format_function_signature(State& state, cst::Function_signature const& signature)
    {
        format(state, "fn {}", signature.name);
        format(state, signature.template_parameters);
        format(state, signature.function_parameters);
        format(state, signature.return_type);
    }

    void format_type_signature(State& state, cst::Type_signature const& signature)
    {
        format(state, "alias {}", signature.name);
        format(state, signature.template_parameters);
        if (signature.concepts_colon_token.has_value()) {
            format(state, ": ");
            format_separated(state, signature.concepts.elements, " + ");
        }
    }

    void format_constructor(State& state, cst::definition::Constructor_body const& body)
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

        void operator()(cst::definition::Function const& function) const
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
                    if (block->result_expression.has_value() and block->side_effects.empty()) {
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

        void operator()(cst::definition::Struct const& structure)
        {
            format(state, "struct {}", structure.name);
            format(state, structure.template_parameters);
            format_constructor(state, structure.body);
        }

        void operator()(cst::definition::Enum const& enumeration)
        {
            format(state, "enum {}", enumeration.name);
            format(state, enumeration.template_parameters);
            format(state, " = ");

            auto const& ctors = enumeration.constructors.elements;
            cpputil::always_assert(not ctors.empty());

            format(state, "{}", ctors.front().name);
            format_constructor(state, ctors.front().body);

            indent(state, [&] {
                for (auto it = ctors.begin() + 1; it != ctors.end(); ++it) {
                    format(state, "{}| {}", newline(state), it->name);
                    format_constructor(state, it->body);
                }
            });
        }

        void operator()(cst::definition::Concept const& concept_)
        {
            format(state, "concept {}", concept_.name);
            format(state, concept_.template_parameters);
            format(state, " {{");
            indent(state, [&] {
                for (auto const& requirement : concept_.requirements) {
                    auto const visitor = utl::Overload {
                        [&](cst::Function_signature const& signature) {
                            format_function_signature(state, signature);
                        },
                        [&](cst::Type_signature const& signature) {
                            format_type_signature(state, signature);
                        },
                    };
                    format(state, "{}", newline(state));
                    std::visit(visitor, requirement);
                }
            });
            format(state, "{}}}", newline(state));
        }

        void operator()(cst::definition::Impl const& implementation)
        {
            format(state, "impl");
            format(state, implementation.template_parameters);
            format(state, " ");
            format(state, implementation.self_type);
            format(state, " {{");
            indent(state, [&] {
                format(state, "{}", newline(state));
                format_definitions(state, implementation.definitions.value);
            });
            format(state, "{}}}", newline(state));
        }

        void operator()(cst::definition::Alias const& alias)
        {
            format(state, "alias {}", alias.name);
            format(state, alias.template_parameters);
            format(state, " = ");
            format(state, alias.type);
        }

        void operator()(cst::definition::Submodule const& module)
        {
            format(state, "module {}", module.name);
            format(state, module.template_parameters);
            format(state, " {{");
            indent(state, [&] {
                format(state, "{}", newline(state));
                format_definitions(state, module.definitions.value);
            });
            format(state, "{}}}", newline(state));
        }
    };
} // namespace

void libformat::format(State& state, cst::Definition const& definition)
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

void kieli::format(
    cst::Arena const&      arena,
    Format_options const&  options,
    cst::Definition const& definition,
    std::string&           output)
{
    State state { arena, options, output };
    libformat::format(state, definition);
}

void kieli::format(
    cst::Arena const&      arena,
    Format_options const&  options,
    cst::Expression const& expression,
    std::string&           output)
{
    State state { arena, options, output };
    libformat::format(state, expression);
}

void kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Pattern const&   pattern,
    std::string&          output)
{
    State state { arena, options, output };
    libformat::format(state, pattern);
}

void kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Type const&      type,
    std::string&          output)
{
    State state { arena, options, output };
    libformat::format(state, type);
}

void kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Expression_id    expression_id,
    std::string&          output)
{
    kieli::format(arena, options, arena.expressions[expression_id], output);
}

void kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Pattern_id       pattern_id,
    std::string&          output)
{
    kieli::format(arena, options, arena.patterns[pattern_id], output);
}

void kieli::format(
    cst::Arena const&     arena,
    Format_options const& options,
    cst::Type_id          type_id,
    std::string&          output)
{
    kieli::format(arena, options, arena.types[type_id], output);
}
