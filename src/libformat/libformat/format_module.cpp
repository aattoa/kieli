#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>
#include <libformat/format.hpp>

using namespace ki;
using namespace ki::fmt;

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
        format(state, "fn {}", state.pool.get(signature.name.id));
        format(state, signature.template_parameters);
        format(state, signature.function_parameters);
        format(state, signature.return_type);
    }

    void format_type_signature(State& state, cst::Type_signature const& signature)
    {
        format(state, "alias {}", state.pool.get(signature.name.id));
        format(state, signature.template_parameters);
        if (signature.concepts_colon_token.has_value()) {
            format(state, ": ");
            format_separated(state, signature.concepts.elements, " + ");
        }
    }

    void format_constructor(State& state, cst::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](cst::Struct_constructor const& constructor) {
                format(state, " {{ ");
                format_comma_separated(state, constructor.fields.value.elements);
                format(state, " }}");
            },
            [&](cst::Tuple_constructor const& constructor) {
                format(state, "(");
                format_comma_separated(state, constructor.types.value.elements);
                format(state, ")");
            },
            [&](cst::Unit_constructor const&) {},
        };
        std::visit(visitor, body);
    }

    struct Definition_format_visitor {
        State& state;

        void operator()(cst::Function const& function) const
        {
            format_function_signature(state, function.signature);
            cst::Expression const& body = state.arena.expressions[function.body];

            switch (state.options.function_body) {
            case Function_body::Leave_as_is:
            {
                if (function.equals_sign_token.has_value()) {
                    format(state, " = ");
                    format(state, body);
                }
                else {
                    format(state, " ");
                    format(state, body);
                }
                return;
            }
            case Function_body::Normalize_to_equals_sign:
            {
                if (auto const* const block = std::get_if<cst::expr::Block>(&body.variant)) {
                    if (block->result.has_value() and block->effects.empty()) {
                        format(state, " = ");
                        format(state, block->result.value());
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
            case Function_body::Normalize_to_block:
            {
                if (std::holds_alternative<cst::expr::Block>(body.variant)) {
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

        void operator()(cst::Struct const& structure)
        {
            format(state, "struct {}", state.pool.get(structure.constructor.name.id));
            format(state, structure.template_parameters);
            format_constructor(state, structure.constructor.body);
        }

        void operator()(cst::Enum const& enumeration)
        {
            format(state, "enum {}", state.pool.get(enumeration.name.id));
            format(state, enumeration.template_parameters);
            format(state, " = ");

            auto const& ctors = enumeration.constructors.elements;
            cpputil::always_assert(not ctors.empty());

            format(state, "{}", state.pool.get(ctors.front().name.id));
            format_constructor(state, ctors.front().body);

            indent(state, [&] {
                for (auto it = ctors.begin() + 1; it != ctors.end(); ++it) {
                    format(state, "{}| {}", newline(state), state.pool.get(it->name.id));
                    format_constructor(state, it->body);
                }
            });
        }

        void operator()(cst::Concept const& concept_)
        {
            format(state, "concept {}", state.pool.get(concept_.name.id));
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

        void operator()(cst::Impl const& implementation)
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

        void operator()(cst::Alias const& alias)
        {
            format(state, "alias {}", state.pool.get(alias.name.id));
            format(state, alias.template_parameters);
            format(state, " = ");
            format(state, alias.type);
        }

        void operator()(cst::Submodule const& module)
        {
            format(state, "module {}", state.pool.get(module.name.id));
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

void ki::fmt::format(State& state, cst::Definition const& definition)
{
    std::visit(Definition_format_visitor { state }, definition.variant);
}

auto ki::fmt::format_module(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Module const&      module) -> std::string
{
    std::string output;

    State state { .pool = pool, .arena = arena, .options = options, .output = output };
    format_definitions(state, module.definitions);

    output.push_back('\n');
    return output;
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Definition const&  definition,
    std::string&            output)
{
    State state { .pool = pool, .arena = arena, .options = options, .output = output };
    format(state, definition);
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Expression const&  expression,
    std::string&            output)
{
    State state { .pool = pool, .arena = arena, .options = options, .output = output };
    format(state, expression);
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Pattern const&     pattern,
    std::string&            output)
{
    State state { .pool = pool, .arena = arena, .options = options, .output = output };
    format(state, pattern);
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Type const&        type,
    std::string&            output)
{
    State state { .pool = pool, .arena = arena, .options = options, .output = output };
    format(state, type);
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Expression_id      expression_id,
    std::string&            output)
{
    format(pool, arena, options, arena.expressions[expression_id], output);
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Pattern_id         pattern_id,
    std::string&            output)
{
    format(pool, arena, options, arena.patterns[pattern_id], output);
}

void ki::fmt::format(
    utl::String_pool const& pool,
    cst::Arena const&       arena,
    Options const&          options,
    cst::Type_id            type_id,
    std::string&            output)
{
    format(pool, arena, options, arena.types[type_id], output);
}
