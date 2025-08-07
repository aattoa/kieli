#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto resolve_function_parameter(
        db::Database&                  db,
        Context&                       ctx,
        Block_state&                   state,
        db::Environment_id             env_id,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        auto pattern
            = resolve_pattern(db, ctx, state, env_id, ctx.arena.ast.patterns[parameter.pattern]);
        auto type_id = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[parameter.type]);

        require_subtype_relationship(
            db,
            ctx,
            state,
            pattern.range,
            ctx.arena.hir.types[pattern.type_id],
            ctx.arena.hir.types[type_id]);

        auto const resolve_default = [&](ast::Expression_id const argument) {
            hir::Expression expression
                = resolve_expression(db, ctx, state, env_id, ctx.arena.ast.expressions[argument]);
            require_subtype_relationship(
                db,
                ctx,
                state,
                expression.range,
                ctx.arena.hir.types[expression.type_id],
                ctx.arena.hir.types[type_id]);
            return ctx.arena.hir.expressions.push(std::move(expression));
        };

        return hir::Function_parameter {
            .pattern_id       = ctx.arena.hir.patterns.push(std::move(pattern)),
            .type_id          = type_id,
            .default_argument = parameter.default_argument.transform(resolve_default),
        };
    }

    void resolve_signature(
        db::Database&                  db,
        Context&                       ctx,
        hir::Function_id const         fun_id,
        db::Environment_id const       env_id,
        ast::Function_signature const& signature)
    {
        auto state            = Block_state {};
        auto signature_env_id = new_scope(ctx, env_id);

        auto template_parameters = resolve_template_parameters(
            db, ctx, state, signature_env_id, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(db, ctx, state, signature_env_id, parameter);
        };

        auto parameters = std::ranges::to<std::vector>(
            std::views::transform(signature.function_parameters, resolve_parameter));
        auto parameter_types = std::ranges::to<std::vector>(
            std::views::transform(parameters, &hir::Function_parameter::type_id));

        auto const return_type = resolve_type(
            db, ctx, state, signature_env_id, ctx.arena.ast.types[signature.return_type]);

        auto const function_type_id = ctx.arena.hir.types.push(
            hir::type::Function {
                .parameter_types = std::move(parameter_types),
                .return_type     = return_type,
            });

        ensure_no_unsolved_variables(db, ctx, state);

        ctx.signature_scope_map.insert_or_assign(fun_id, signature_env_id);

        ctx.arena.hir.functions[fun_id].signature = hir::Function_signature {
            .template_parameters = std::move(template_parameters),
            .parameters          = std::move(parameters),
            .return_type_id      = return_type,
            .function_type_id    = function_type_id,
            .name                = signature.name,
        };
    }

    auto resolve_constructor(
        db::Database&           db,
        Context&                ctx,
        db::Environment_id      env_id,
        hir::Type_id            owner_type_id,
        ast::Constructor const& constructor,
        std::size_t             discriminant) -> hir::Constructor_id
    {
        auto state = Block_state {};

        auto const visitor = utl::Overload {
            [&](ast::Unit_constructor) -> hir::Constructor_body {
                return hir::Unit_constructor {};
            },
            [&](ast::Tuple_constructor const& tuple) -> hir::Constructor_body {
                auto resolve = [&](ast::Type_id type_id) {
                    return resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[type_id]);
                };

                auto field_types
                    = std::ranges::to<std::vector>(std::views::transform(tuple.types, resolve));

                hir::type::Function function_type {
                    .parameter_types = field_types,
                    .return_type     = owner_type_id,
                };

                return hir::Tuple_constructor {
                    .types            = std::move(field_types),
                    .function_type_id = ctx.arena.hir.types.push(std::move(function_type)),
                };
            },
            [&](ast::Struct_constructor const& structure) -> hir::Constructor_body {
                hir::Struct_constructor body;
                for (auto [index, field] : utl::enumerate(structure.fields)) {
                    if (body.fields.contains(field.name.id)) {
                        auto message = std::format(
                            "Duplicate struct field '{}'", db.string_pool.get(field.name.id));
                        ctx.add_diagnostic(lsp::error(field.name.range, std::move(message)));
                        continue;
                    }

                    auto const symbol_id = new_symbol(ctx, field.name, db::Error {});

                    auto const field_type
                        = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[field.type]);

                    auto const field_id = ctx.arena.hir.fields.push(
                        hir::Field_info {
                            .name        = field.name,
                            .type_id     = field_type,
                            .symbol_id   = symbol_id,
                            .field_index = cpputil::num::safe_cast<std::size_t>(index),
                        });

                    ctx.arena.symbols[symbol_id].variant = field_id;

                    body.fields.insert_or_assign(field.name.id, field_id);
                    db::add_reference(db, ctx.doc_id, lsp::write(field.name.range), symbol_id);
                }
                return body;
            },
        };

        auto body = std::visit(visitor, constructor.body);
        ensure_no_unsolved_variables(db, ctx, state);

        return ctx.arena.hir.constructors.push(
            hir::Constructor_info {
                .body          = std::move(body),
                .name          = constructor.name,
                .owner_type_id = owner_type_id,
                .discriminant  = discriminant,
            });
    }
} // namespace

auto ki::res::resolve_function_body(db::Database& db, Context& ctx, hir::Function_id id)
    -> hir::Expression_id
{
    hir::Function_info& info = ctx.arena.hir.functions[id];

    if (not info.body_id.has_value()) {
        auto  state     = Block_state {};
        auto& signature = resolve_function_signature(db, ctx, id);

        auto const it = ctx.signature_scope_map.find(id);
        cpputil::always_assert(it != ctx.signature_scope_map.end());
        hir::Expression body = resolve_expression(
            db, ctx, state, it->second, ctx.arena.ast.expressions[info.ast.body]);
        report_unused(db, ctx, it->second);
        ctx.signature_scope_map.erase(it);

        require_subtype_relationship(
            db,
            ctx,
            state,
            body.range,
            ctx.arena.hir.types[body.type_id],
            ctx.arena.hir.types[signature.return_type_id]);
        ensure_no_unsolved_variables(db, ctx, state);

        info.body_id = ctx.arena.hir.expressions.push(std::move(body));
    }
    return info.body_id.value();
}

auto ki::res::resolve_function_signature(db::Database& db, Context& ctx, hir::Function_id id)
    -> hir::Function_signature&
{
    hir::Function_info& info = ctx.arena.hir.functions[id];
    if (not info.signature.has_value()) {
        resolve_signature(db, ctx, id, info.env_id, info.ast.signature);
    }
    return info.signature.value();
}

auto ki::res::resolve_structure(db::Database& db, Context& ctx, hir::Structure_id id)
    -> hir::Structure&
{
    hir::Structure_info& info = ctx.arena.hir.structures[id];
    if (not info.hir.has_value()) {
        auto const ctor_env_id = ctx.arena.environments.push(
            db::Environment {
                .map       = {},
                .parent_id = info.env_id,
                .name_id   = info.name.id,
                .doc_id    = ctx.doc_id,
                .kind      = db::Environment_kind::Type,
            });

        auto parameters_state    = Block_state {};
        auto template_parameters = resolve_template_parameters(
            db, ctx, parameters_state, ctor_env_id, info.ast.template_parameters);
        (void)template_parameters; // TODO
        ensure_no_unsolved_variables(db, ctx, parameters_state);

        db::Environment associated_env {
            .map       = {},
            .parent_id = info.env_id,
            .name_id   = info.name.id,
            .doc_id    = ctx.doc_id,
            .kind      = db::Environment_kind::Type,
        };

        info.hir = hir::Structure {
            .constructor_id = resolve_constructor(
                db, ctx, ctor_env_id, info.type_id, info.ast.constructor, /*discriminant=*/0),
            .associated_env_id = ctx.arena.environments.push(std::move(associated_env)),
        };
    }
    return info.hir.value();
}

auto ki::res::resolve_enumeration(db::Database& db, Context& ctx, hir::Enumeration_id id)
    -> hir::Enumeration&
{
    hir::Enumeration_info& info = ctx.arena.hir.enumerations[id];
    if (not info.hir.has_value()) {
        auto const ctor_env_id = ctx.arena.environments.push(
            db::Environment {
                .map       = {},
                .parent_id = info.env_id,
                .name_id   = info.name.id,
                .doc_id    = ctx.doc_id,
                .kind      = db::Environment_kind::Type,
            });

        auto parameters_state    = Block_state {};
        auto template_parameters = resolve_template_parameters(
            db, ctx, parameters_state, ctor_env_id, info.ast.template_parameters);
        (void)template_parameters; // TODO
        ensure_no_unsolved_variables(db, ctx, parameters_state);

        auto const associated_env_id = ctx.arena.environments.push(
            db::Environment {
                .map       = {},
                .parent_id = info.env_id,
                .name_id   = info.name.id,
                .doc_id    = ctx.doc_id,
                .kind      = db::Environment_kind::Type,
            });

        std::vector<db::Symbol_id> constructor_ids;
        constructor_ids.reserve(info.ast.constructors.size());

        for (auto [discriminant, constructor] : utl::enumerate(info.ast.constructors)) {
            auto symbol_id = bind_symbol(
                db,
                ctx,
                associated_env_id,
                constructor.name,
                resolve_constructor(db, ctx, ctor_env_id, info.type_id, constructor, discriminant));
            constructor_ids.push_back(symbol_id);
        }

        info.hir = hir::Enumeration {
            .constructor_ids   = std::move(constructor_ids),
            .associated_env_id = associated_env_id,
        };
    }
    return info.hir.value();
}

auto ki::res::resolve_concept(db::Database& db, Context& ctx, hir::Concept_id id) -> hir::Concept&
{
    (void)db;

    hir::Concept_info& info = ctx.arena.hir.concepts[id];
    if (not info.hir.has_value()) {
        std::string message = "Concept resolution has not been implemented yet";
        ctx.add_diagnostic(lsp::error(info.name.range, std::move(message)));
        info.hir = hir::Concept {};
    }
    return info.hir.value();
}

auto ki::res::resolve_alias(db::Database& db, Context& ctx, hir::Alias_id id) -> hir::Alias&
{
    hir::Alias_info& info = ctx.arena.hir.aliases[id];
    if (not info.hir.has_value()) {
        auto state = Block_state {};
        auto type  = resolve_type(db, ctx, state, info.env_id, ctx.arena.ast.types[info.ast.type]);
        ensure_no_unsolved_variables(db, ctx, state);
        info.hir = hir::Alias { .name = info.name, .type_id = type };
    }
    return info.hir.value();
}
