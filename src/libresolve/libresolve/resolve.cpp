#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libcompiler/ast/display.hpp>

auto ki::res::context(db::Document_id doc_id) -> Context
{
    auto arena = db::Arena {};

    auto constants = make_constants(arena.hir);

    auto env_id = arena.environments.push(
        db::Environment {
            .map       = {},
            .parent_id = std::nullopt,
            .name_id   = std::nullopt,
            .doc_id    = doc_id,
            .kind      = db::Environment_kind::Root,
        });

    return Context {
        .arena               = std::move(arena),
        .constants           = constants,
        .signature_scope_map = {},
        .root_env_id         = env_id,
        .doc_id              = doc_id,
        .tags                = {},
    };
}

auto ki::res::make_constants(hir::Arena& arena) -> Constants
{
    return Constants {
        .type_i8        = arena.types.push(hir::type::Integer::I8),
        .type_i16       = arena.types.push(hir::type::Integer::I16),
        .type_i32       = arena.types.push(hir::type::Integer::I32),
        .type_i64       = arena.types.push(hir::type::Integer::I64),
        .type_u8        = arena.types.push(hir::type::Integer::U8),
        .type_u16       = arena.types.push(hir::type::Integer::U16),
        .type_u32       = arena.types.push(hir::type::Integer::U32),
        .type_u64       = arena.types.push(hir::type::Integer::U64),
        .type_boolean   = arena.types.push(hir::type::Boolean {}),
        .type_floating  = arena.types.push(hir::type::Floating {}),
        .type_string    = arena.types.push(hir::type::String {}),
        .type_character = arena.types.push(hir::type::Character {}),
        .type_unit      = arena.types.push(hir::type::Tuple {}),
        .type_error     = arena.types.push(db::Error {}),
        .mut_yes        = arena.mutabilities.push(db::Mutability::Mut),
        .mut_no         = arena.mutabilities.push(db::Mutability::Immut),
        .mut_error      = arena.mutabilities.push(db::Error {}),
    };
}

auto ki::res::fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag
{
    return hir::Template_parameter_tag { ++tags.current_template_parameter_tag };
}

auto ki::res::error_type(Context const& ctx, lsp::Range range) -> hir::Type
{
    return hir::Type { .id = ctx.constants.type_error, .range = range };
}

auto ki::res::error_expression(Context const& ctx, lsp::Range range) -> hir::Expression
{
    return hir::Expression {
        .variant  = db::Error {},
        .type_id  = ctx.constants.type_error,
        .mut_id   = ctx.constants.mut_yes,
        .category = hir::Expression_category::Place,
        .range    = range,
    };
}

auto ki::res::unit_expression(Context const& ctx, lsp::Range range) -> hir::Expression
{
    return hir::Expression {
        .variant  = hir::expr::Tuple {},
        .type_id  = ctx.constants.type_unit,
        .mut_id   = ctx.constants.mut_no,
        .category = hir::Expression_category::Value,
        .range    = range,
    };
}

auto ki::res::new_scope(Context& ctx, db::Environment_id parent_id) -> db::Environment_id
{
    return ctx.arena.environments.push(
        db::Environment {
            .map       = {},
            .parent_id = parent_id,
            .name_id   = std::nullopt,
            .doc_id    = ctx.doc_id,
            .kind      = db::Environment_kind::Scope,
        });
}

auto ki::res::new_symbol(Context& ctx, db::Name name, db::Symbol_variant variant) -> db::Symbol_id
{
    return ctx.arena.symbols.push(db::Symbol { .variant = variant, .name = name, .use_count = 0 });
}

void ki::res::warn_if_unused(db::Database& db, Context& ctx, db::Symbol_id symbol_id)
{
    auto const& symbol = ctx.arena.symbols[symbol_id];
    auto const  name   = db.string_pool.get(symbol.name.id);
    if (symbol.use_count == 0 and not name.starts_with('_')) {
        auto message = std::format(
            "'{0}' is unused. If this is intentional, prefix it with an underscore: '_{0}'", name);

        lsp::Diagnostic warning {
            .message      = std::move(message),
            .range        = symbol.name.range,
            .severity     = lsp::Severity::Warning,
            .related_info = {},
            .tag          = lsp::Diagnostic_tag::Unnecessary,
        };

        db::add_diagnostic(db, ctx.doc_id, std::move(warning));
        db::add_action(db, ctx.doc_id, symbol.name.range, db::Action_silence_unused { symbol_id });
    }
}

void ki::res::report_unused(db::Database& db, Context& ctx, db::Environment_id env_id)
{
    for (db::Symbol_id symbol_id : std::views::values(ctx.arena.environments[env_id].map)) {
        warn_if_unused(db, ctx, symbol_id);
    }
}

auto ki::res::can_shadow(db::Symbol_variant variant) -> bool
{
    return std::holds_alternative<hir::Local_variable_id>(variant)
        or std::holds_alternative<hir::Local_type_id>(variant);
}

auto ki::res::bind_symbol(
    db::Database&      db,
    Context&           ctx,
    db::Environment_id env_id,
    db::Name           name,
    db::Symbol_variant variant) -> db::Symbol_id
{
    auto&      env       = ctx.arena.environments[env_id];
    auto const symbol_id = new_symbol(ctx, name, variant);

    if (auto const it = env.map.find(name.id); it != env.map.end()) {
        if (can_shadow(ctx.arena.symbols[it->second].variant)) {
            warn_if_unused(db, ctx, it->second);
        }
        else {
            auto message = std::format("Redefinition of '{}'", db.string_pool.get(name.id));
            db::add_error(db, ctx.doc_id, name.range, std::move(message));
            return symbol_id;
        }
    }

    env.map.insert_or_assign(name.id, symbol_id);
    db::add_reference(db, ctx.doc_id, lsp::write(name.range), symbol_id);
    return symbol_id;
}

void ki::res::flatten_type(Context& ctx, Block_state& state, hir::Type_variant& type)
{
    if (auto const* variable = std::get_if<hir::type::Variable>(&type)) {
        Type_variable_data& data = state.type_vars.at(variable->id.get());
        assert(variable->id == data.var_id);
        if (data.is_solved) {
            type           = ctx.arena.hir.types[data.type_id];
            data.is_solved = true;
            return;
        }
        std::size_t index = state.type_var_set.find(data.var_id.get());
        if (data.var_id.get() == index) {
            return;
        }
        Type_variable_data& repr_data = state.type_vars.at(index);
        hir::Type_variant&  repr_type = ctx.arena.hir.types[repr_data.type_id];
        flatten_type(ctx, state, repr_type);
        if (repr_data.is_solved) {
            type           = repr_type;
            data.is_solved = true;
            return;
        }
    }
}

void ki::res::set_type_solution(
    db::Database&         db,
    Context&              ctx,
    Block_state&          state,
    hir::Type_variable_id var_id,
    hir::Type_variant     solution)
{
    auto& repr_data = state.type_vars.at(state.type_var_set.find(var_id.get()));
    auto& repr_type = ctx.arena.hir.types[repr_data.type_id];
    if (repr_data.is_solved) {
        require_subtype_relationship(
            db, ctx, state, state.type_vars.at(var_id.get()).origin, solution, repr_type);
    }
    repr_type           = std::move(solution);
    repr_data.is_solved = true;
}

void ki::res::set_mut_solution(
    db::Database&               db,
    Context&                    ctx,
    Block_state&                state,
    hir::Mutability_variable_id var_id,
    hir::Mutability_variant     solution)
{
    auto& repr = state.mut_vars.at(state.mut_var_set.find(var_id.get()));

    if (repr.is_solved) {
        require_submutability_relationship(
            db,
            ctx,
            state,
            state.mut_vars.at(var_id.get()).origin,
            solution,
            ctx.arena.hir.mutabilities[repr.mut_id]);
    }

    ctx.arena.hir.mutabilities[repr.mut_id] = std::move(solution);
    repr.is_solved                          = true;
}

auto ki::res::fresh_general_type_variable(Context& ctx, Block_state& state, lsp::Range origin)
    -> hir::Type
{
    auto const var_id  = hir::Type_variable_id { state.type_vars.size() };
    auto const type_id = ctx.arena.hir.types.push(hir::type::Variable { var_id });
    state.type_vars.push_back(
        Type_variable_data {
            .var_id  = var_id,
            .type_id = type_id,
            .origin  = origin,
            .kind    = hir::Type_variable_kind::General,
        });
    (void)state.type_var_set.add();
    return hir::Type { .id = type_id, .range = origin };
}

auto ki::res::fresh_integral_type_variable(Context& ctx, Block_state& state, lsp::Range origin)
    -> hir::Type
{
    auto const var_id  = hir::Type_variable_id { state.type_vars.size() };
    auto const type_id = ctx.arena.hir.types.push(hir::type::Variable { var_id });
    state.type_vars.push_back(
        Type_variable_data {
            .var_id  = var_id,
            .type_id = type_id,
            .origin  = origin,
            .kind    = hir::Type_variable_kind::Integral,
        });
    (void)state.type_var_set.add();
    return hir::Type { .id = type_id, .range = origin };
}

auto ki::res::fresh_mutability_variable(Context& ctx, Block_state& state, lsp::Range origin)
    -> hir::Mutability
{
    auto const var_id = hir::Mutability_variable_id { state.mut_vars.size() };
    auto const mut_id = ctx.arena.hir.mutabilities.push(hir::mut::Variable { var_id });
    state.mut_vars.push_back(
        Mutability_variable_data {
            .var_id = var_id,
            .mut_id = mut_id,
            .origin = origin,
        });
    (void)state.mut_var_set.add();
    return hir::Mutability { .id = mut_id, .range = origin };
}

void ki::res::ensure_no_unsolved_variables(db::Database& db, Context& ctx, Block_state& state)
{
    for (Mutability_variable_data& data : state.mut_vars) {
        if (not data.is_solved) {
            set_mut_solution(db, ctx, state, data.var_id, db::Mutability::Immut);
        }
    }
    for (Type_variable_data& data : state.type_vars) {
        flatten_type(ctx, state, ctx.arena.hir.types[data.type_id]);
        if (not data.is_solved) {
            if (data.kind == hir::Type_variable_kind::Integral) {
                set_type_solution(db, ctx, state, data.var_id, hir::type::Integer::I32);
            }
            else {
                auto message = std::format("Unsolved type variable: ?{}", data.var_id.get());
                db::add_error(db, ctx.doc_id, data.origin, std::move(message));
                set_type_solution(db, ctx, state, data.var_id, db::Error {});
            }
        }
    }
}

void ki::res::resolve_symbol(db::Database& db, Context& ctx, db::Symbol_id symbol_id)
{
    auto const visitor = utl::Overload {
        [&](hir::Function_id id) { resolve_function_body(db, ctx, id); },
        [&](hir::Structure_id id) { resolve_structure(db, ctx, id); },
        [&](hir::Enumeration_id id) { resolve_enumeration(db, ctx, id); },
        [&](hir::Concept_id id) { resolve_concept(db, ctx, id); },
        [&](hir::Alias_id id) { resolve_alias(db, ctx, id); },

        [&](hir::Module_id) {
            // Modules do not need to be separately resolved.
        },

        [](hir::Constructor_id) { cpputil::unreachable(); },
        [](hir::Field_id) { cpputil::unreachable(); },
        [](hir::Local_variable_id) { cpputil::unreachable(); },
        [](hir::Local_type_id) { cpputil::unreachable(); },
        [](hir::Local_mutability_id) { cpputil::unreachable(); },
        [](db::Error) { cpputil::unreachable(); },
    };
    std::visit(visitor, ctx.arena.symbols[symbol_id].variant);
}
