#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>
#include <libformat/format.hpp>
#include <libparse/parse.hpp>

using namespace ki;
using namespace ki::fmt;

// TODO: collapse string literals, expand integer literals, insert digit separators

namespace {
    void format_function_signature(Context& ctx, cst::Function_signature const& signature)
    {
        std::print(ctx.stream, "fn {}", ctx.db.string_pool.get(signature.name.id));
        format(ctx, signature.template_parameters);
        format(ctx, signature.function_parameters);
        format(ctx, signature.return_type);
    }

    void format_type_signature(Context& ctx, cst::Type_signature const& signature)
    {
        std::print(ctx.stream, "alias {}", ctx.db.string_pool.get(signature.name.id));
        format(ctx, signature.template_parameters);
        if (signature.concepts_colon_token.has_value()) {
            std::print(ctx.stream, ": ");
            format_separated(ctx, signature.concepts.elements, " + ");
        }
    }

    void format_constructor(Context& ctx, cst::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](cst::Struct_constructor const& constructor) {
                std::print(ctx.stream, " {{ ");
                format_comma_separated(ctx, constructor.fields.value.elements);
                std::print(ctx.stream, " }}");
            },
            [&](cst::Tuple_constructor const& constructor) {
                std::print(ctx.stream, "(");
                format_comma_separated(ctx, constructor.types.value.elements);
                std::print(ctx.stream, ")");
            },
            [&](cst::Unit_constructor const&) {},
        };
        std::visit(visitor, body);
    }
} // namespace

void ki::fmt::format(Context& ctx, cst::Function const& function)
{
    indent(ctx);
    format_function_signature(ctx, function.signature);
    cst::Expression const& body = ctx.arena.expressions[function.body];

    switch (ctx.options.function_body) {
    case Function_body::Leave_as_is:
        if (function.equals_sign_token.has_value()) {
            std::print(ctx.stream, " = ");
            format(ctx, body);
        }
        else {
            std::print(ctx.stream, " ");
            format(ctx, body);
        }
        return;
    case Function_body::Normalize_to_equals_sign:
        if (auto const* const block = std::get_if<cst::expr::Block>(&body.variant)) {
            if (block->result.has_value() and block->effects.empty()) {
                std::print(ctx.stream, " = ");
                format(ctx, block->result.value());
            }
            else {
                std::print(ctx.stream, " ");
                format(ctx, function.body);
            }
        }
        else {
            std::print(ctx.stream, " = ");
            format(ctx, function.body);
        }
        return;
    case Function_body::Normalize_to_block:
        if (std::holds_alternative<cst::expr::Block>(body.variant)) {
            std::print(ctx.stream, " ");
            format(ctx, function.body);
        }
        else {
            std::print(ctx.stream, " {{ ");
            format(ctx, function.body);
            std::print(ctx.stream, " }}");
        }
        return;
    }

    cpputil::unreachable();
}

void ki::fmt::format(Context& ctx, cst::Struct const& structure)
{
    indent(ctx);
    std::print(ctx.stream, "struct {}", ctx.db.string_pool.get(structure.constructor.name.id));
    format(ctx, structure.template_parameters);
    format_constructor(ctx, structure.constructor.body);
}

void ki::fmt::format(Context& ctx, cst::Enum const& enumeration)
{
    indent(ctx);
    std::print(ctx.stream, "enum {}", ctx.db.string_pool.get(enumeration.name.id));
    format(ctx, enumeration.template_parameters);
    std::print(ctx.stream, " = ");

    auto const& ctors = enumeration.constructors.elements;
    cpputil::always_assert(not ctors.empty());

    std::print(ctx.stream, "{}", ctx.db.string_pool.get(ctors.front().name.id));
    format_constructor(ctx, ctors.front().body);

    indent(ctx, [&] {
        for (auto it = ctors.begin() + 1; it != ctors.end(); ++it) {
            newline(ctx);
            std::print(ctx.stream, "| {}", ctx.db.string_pool.get(it->name.id));
            format_constructor(ctx, it->body);
        }
    });
}

void ki::fmt::format(Context& ctx, cst::Alias const& alias)
{
    indent(ctx);
    std::print(ctx.stream, "alias {}", ctx.db.string_pool.get(alias.name.id));
    format(ctx, alias.template_parameters);
    std::print(ctx.stream, " = ");
    format(ctx, alias.type);
}

void ki::fmt::format(Context& ctx, cst::Concept const& concept_)
{
    indent(ctx);
    std::print(ctx.stream, "concept {}", ctx.db.string_pool.get(concept_.name.id));
    format(ctx, concept_.template_parameters);
    std::print(ctx.stream, " {{");
    indent(ctx, [&] {
        for (auto const& requirement : concept_.requirements) {
            auto const visitor = utl::Overload {
                [&](cst::Function_signature const& signature) {
                    format_function_signature(ctx, signature);
                },
                [&](cst::Type_signature const& signature) {
                    format_type_signature(ctx, signature);
                },
            };
            newline(ctx);
            std::visit(visitor, requirement);
        }
    });
    newline(ctx);
    std::print(ctx.stream, "}}");
}

void ki::fmt::format(Context& ctx, cst::Impl_begin const& impl)
{
    indent(ctx);
    std::print(ctx.stream, "impl");
    format(ctx, impl.template_parameters);
    std::print(ctx.stream, " ");
    format(ctx, impl.self_type);
    std::print(ctx.stream, " {{");
    ++ctx.indentation;
    ctx.did_open_block = true;
}

void ki::fmt::format(Context& ctx, cst::Submodule_begin const& module)
{
    indent(ctx);
    std::print(ctx.stream, "module {} {{", ctx.db.string_pool.get(module.name.id));
    ++ctx.indentation;
    ctx.did_open_block = true;
}

void ki::fmt::format(Context& ctx, cst::Block_end const&)
{
    --ctx.indentation;
    newline(ctx);
    std::print(ctx.stream, "}}");
}

auto ki::fmt::format_document(
    std::ostream& stream, db::Database& db, db::Document_id doc_id, Options const& options)
    -> lsp::Range
{
    auto par_ctx = par::context(db, doc_id);

    auto fmt_ctx = Context {
        .db      = db,
        .arena   = par_ctx.arena,
        .stream  = stream,
        .options = options,
    };

    par::parse(par_ctx, [&](auto const& definition) { format(fmt_ctx, definition); });

    std::print(stream, "\n");

    return lsp::Range(lsp::Position {}, par_ctx.lex_state.position);
}
