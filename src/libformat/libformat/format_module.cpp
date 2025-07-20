#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>
#include <libformat/format.hpp>

using namespace ki;
using namespace ki::fmt;

// TODO: collapse string literals, expand integer literals, insert digit separators

namespace {
    auto do_format(Context& ctx, auto const& x) -> std::string
    {
        State state { .ctx = ctx, .output = {} };
        ki::fmt::format(state, x);
        return std::move(state.output);
    }

    void format_function_signature(State& state, cst::Function_signature const& signature)
    {
        format(state, "fn {}", state.ctx.db.string_pool.get(signature.name.id));
        format(state, signature.template_parameters);
        format(state, signature.function_parameters);
        format(state, signature.return_type);
    }

    void format_type_signature(State& state, cst::Type_signature const& signature)
    {
        format(state, "alias {}", state.ctx.db.string_pool.get(signature.name.id));
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
} // namespace

void ki::fmt::format(State& state, cst::Function const& function)
{
    format_function_signature(state, function.signature);
    cst::Expression const& body = state.ctx.arena.expressions[function.body];

    switch (state.ctx.options.function_body) {
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

void ki::fmt::format(State& state, cst::Struct const& structure)
{
    format(state, "struct {}", state.ctx.db.string_pool.get(structure.constructor.name.id));
    format(state, structure.template_parameters);
    format_constructor(state, structure.constructor.body);
}

void ki::fmt::format(State& state, cst::Enum const& enumeration)
{
    format(state, "enum {}", state.ctx.db.string_pool.get(enumeration.name.id));
    format(state, enumeration.template_parameters);
    format(state, " = ");

    auto const& ctors = enumeration.constructors.elements;
    cpputil::always_assert(not ctors.empty());

    format(state, "{}", state.ctx.db.string_pool.get(ctors.front().name.id));
    format_constructor(state, ctors.front().body);

    indent(state, [&] {
        for (auto it = ctors.begin() + 1; it != ctors.end(); ++it) {
            format(state, "{}| {}", newline(state), state.ctx.db.string_pool.get(it->name.id));
            format_constructor(state, it->body);
        }
    });
}

void ki::fmt::format(State& state, cst::Alias const& alias)
{
    format(state, "alias {}", state.ctx.db.string_pool.get(alias.name.id));
    format(state, alias.template_parameters);
    format(state, " = ");
    format(state, alias.type);
}

void ki::fmt::format(State& state, cst::Concept const& concept_)
{
    format(state, "concept {}", state.ctx.db.string_pool.get(concept_.name.id));
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

void ki::fmt::format(State& state, cst::Impl_begin const& impl)
{
    format(state, "impl");
    format(state, impl.template_parameters);
    format(state, " ");
    format(state, impl.self_type);
    format(state, " {{");
    ++state.ctx.indentation;
}

void ki::fmt::format(State& state, cst::Submodule_begin const& module)
{
    format(state, "module {}", state.ctx.db.string_pool.get(module.name.id));
    format(state, " {{");
    ++state.ctx.indentation;
}

void ki::fmt::format(State& state, cst::Block_end const&)
{
    --state.ctx.indentation;
    format(state, "}}");
}

auto ki::fmt::format(Context& ctx, cst::Function const& function) -> std::string
{
    return do_format(ctx, function);
}

auto ki::fmt::format(Context& ctx, cst::Struct const& structure) -> std::string
{
    return do_format(ctx, structure);
}

auto ki::fmt::format(Context& ctx, cst::Enum const& enumeration) -> std::string
{
    return do_format(ctx, enumeration);
}

auto ki::fmt::format(Context& ctx, cst::Alias const& alias) -> std::string
{
    return do_format(ctx, alias);
}

auto ki::fmt::format(Context& ctx, cst::Concept const& concept_) -> std::string
{
    return do_format(ctx, concept_);
}

auto ki::fmt::format(Context& ctx, cst::Impl_begin const& impl) -> std::string
{
    return do_format(ctx, impl);
}

auto ki::fmt::format(Context& ctx, cst::Submodule_begin const& submodule) -> std::string
{
    return do_format(ctx, submodule);
}

auto ki::fmt::format(Context& ctx, cst::Block_end const& block_end) -> std::string
{
    return do_format(ctx, block_end);
}

auto ki::fmt::format(Context& ctx, cst::Expression const& expression) -> std::string
{
    return do_format(ctx, expression);
}

auto ki::fmt::format(Context& ctx, cst::Pattern const& pattern) -> std::string
{
    return do_format(ctx, pattern);
}

auto ki::fmt::format(Context& ctx, cst::Type const& type) -> std::string
{
    return do_format(ctx, type);
}

auto ki::fmt::format(Context& ctx, cst::Expression_id expression_id) -> std::string
{
    return do_format(ctx, ctx.arena.expressions[expression_id]);
}

auto ki::fmt::format(Context& ctx, cst::Pattern_id pattern_id) -> std::string
{
    return do_format(ctx, ctx.arena.patterns[pattern_id]);
}

auto ki::fmt::format(Context& ctx, cst::Type_id type_id) -> std::string
{
    return do_format(ctx, ctx.arena.types[type_id]);
}
