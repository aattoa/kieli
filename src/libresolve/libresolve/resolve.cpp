#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

auto ki::res::context(db::Document_id doc_id, db::Diagnostic_sink sink) -> Context
{
    auto arena = db::Arena {};

    auto builtins = make_builtins(arena.hir);

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
        .builtins            = builtins,
        .signature_scope_map = {},
        .root_env_id         = env_id,
        .doc_id              = doc_id,
        .add_diagnostic      = sink,
        .tags                = {},
    };
}

auto ki::res::make_builtins(hir::Arena& arena) -> Builtins
{
    return Builtins {
        .type_error  = arena.types.push(db::Error {}),
        .type_never  = arena.types.push(hir::type::Builtin::Never),
        .type_char   = arena.types.push(hir::type::Builtin::Char),
        .type_string = arena.types.push(hir::type::Builtin::String),
        .type_unit   = arena.types.push(hir::type::Tuple {}),
        .type_bool   = arena.types.push(hir::type::Builtin::Bool),
        .type_i8     = arena.types.push(hir::type::Builtin::I8),
        .type_i16    = arena.types.push(hir::type::Builtin::I16),
        .type_i32    = arena.types.push(hir::type::Builtin::I32),
        .type_i64    = arena.types.push(hir::type::Builtin::I64),
        .type_u8     = arena.types.push(hir::type::Builtin::U8),
        .type_u16    = arena.types.push(hir::type::Builtin::U16),
        .type_u32    = arena.types.push(hir::type::Builtin::U32),
        .type_u64    = arena.types.push(hir::type::Builtin::U64),
        .type_f32    = arena.types.push(hir::type::Builtin::F32),
        .type_f64    = arena.types.push(hir::type::Builtin::F64),
        .mut_yes     = arena.mutabilities.push(db::Mutability::Mut),
        .mut_no      = arena.mutabilities.push(db::Mutability::Immut),
        .mut_error   = arena.mutabilities.push(db::Error {}),
    };
}

auto ki::res::builtin_type_id(Builtins const& builtins, hir::type::Builtin builtin) -> hir::Type_id
{
    switch (builtin) {
    case hir::type::Builtin::I8:     return builtins.type_i8;
    case hir::type::Builtin::I16:    return builtins.type_i16;
    case hir::type::Builtin::I32:    return builtins.type_i32;
    case hir::type::Builtin::I64:    return builtins.type_i64;
    case hir::type::Builtin::U8:     return builtins.type_u8;
    case hir::type::Builtin::U16:    return builtins.type_u16;
    case hir::type::Builtin::U32:    return builtins.type_u32;
    case hir::type::Builtin::U64:    return builtins.type_u64;
    case hir::type::Builtin::F32:    return builtins.type_f32;
    case hir::type::Builtin::F64:    return builtins.type_f64;
    case hir::type::Builtin::Bool:   return builtins.type_bool;
    case hir::type::Builtin::Char:   return builtins.type_char;
    case hir::type::Builtin::String: return builtins.type_string;
    case hir::type::Builtin::Never:  return builtins.type_never;
    }
    cpputil::unreachable();
}

auto ki::res::builtin_expr_type(Context& ctx, hir::expr::Builtin builtin) -> hir::Type_id
{
    switch (builtin) {
    case hir::expr::Builtin::AddI8:
    case hir::expr::Builtin::SubI8:
    case hir::expr::Builtin::MulI8:
    case hir::expr::Builtin::DivI8:
    case hir::expr::Builtin::ModI8:       return arith_bin_op_type(ctx, ctx.builtins.type_i8);
    case hir::expr::Builtin::AddI16:
    case hir::expr::Builtin::SubI16:
    case hir::expr::Builtin::MulI16:
    case hir::expr::Builtin::DivI16:
    case hir::expr::Builtin::ModI16:      return arith_bin_op_type(ctx, ctx.builtins.type_i16);
    case hir::expr::Builtin::AddI32:
    case hir::expr::Builtin::SubI32:
    case hir::expr::Builtin::MulI32:
    case hir::expr::Builtin::DivI32:
    case hir::expr::Builtin::ModI32:      return arith_bin_op_type(ctx, ctx.builtins.type_i32);
    case hir::expr::Builtin::AddI64:
    case hir::expr::Builtin::SubI64:
    case hir::expr::Builtin::MulI64:
    case hir::expr::Builtin::DivI64:
    case hir::expr::Builtin::ModI64:      return arith_bin_op_type(ctx, ctx.builtins.type_i64);
    case hir::expr::Builtin::AddU8:
    case hir::expr::Builtin::SubU8:
    case hir::expr::Builtin::MulU8:
    case hir::expr::Builtin::DivU8:
    case hir::expr::Builtin::ModU8:       return arith_bin_op_type(ctx, ctx.builtins.type_u8);
    case hir::expr::Builtin::AddU16:
    case hir::expr::Builtin::SubU16:
    case hir::expr::Builtin::MulU16:
    case hir::expr::Builtin::DivU16:
    case hir::expr::Builtin::ModU16:      return arith_bin_op_type(ctx, ctx.builtins.type_u16);
    case hir::expr::Builtin::AddU32:
    case hir::expr::Builtin::SubU32:
    case hir::expr::Builtin::MulU32:
    case hir::expr::Builtin::DivU32:
    case hir::expr::Builtin::ModU32:      return arith_bin_op_type(ctx, ctx.builtins.type_u32);
    case hir::expr::Builtin::AddU64:
    case hir::expr::Builtin::SubU64:
    case hir::expr::Builtin::MulU64:
    case hir::expr::Builtin::DivU64:
    case hir::expr::Builtin::ModU64:      return arith_bin_op_type(ctx, ctx.builtins.type_u64);
    case hir::expr::Builtin::AddF32:
    case hir::expr::Builtin::SubF32:
    case hir::expr::Builtin::MulF32:
    case hir::expr::Builtin::DivF32:
    case hir::expr::Builtin::ModF32:      return arith_bin_op_type(ctx, ctx.builtins.type_f32);
    case hir::expr::Builtin::AddF64:
    case hir::expr::Builtin::SubF64:
    case hir::expr::Builtin::MulF64:
    case hir::expr::Builtin::DivF64:
    case hir::expr::Builtin::ModF64:      return arith_bin_op_type(ctx, ctx.builtins.type_f64);
    case hir::expr::Builtin::EqI8:
    case hir::expr::Builtin::LtI8:        return cmp_bin_op_type(ctx, ctx.builtins.type_i8);
    case hir::expr::Builtin::EqI16:
    case hir::expr::Builtin::LtI16:       return cmp_bin_op_type(ctx, ctx.builtins.type_i16);
    case hir::expr::Builtin::EqI32:
    case hir::expr::Builtin::LtI32:       return cmp_bin_op_type(ctx, ctx.builtins.type_i32);
    case hir::expr::Builtin::EqI64:
    case hir::expr::Builtin::LtI64:       return cmp_bin_op_type(ctx, ctx.builtins.type_i64);
    case hir::expr::Builtin::EqU8:
    case hir::expr::Builtin::LtU8:        return cmp_bin_op_type(ctx, ctx.builtins.type_u8);
    case hir::expr::Builtin::EqU16:
    case hir::expr::Builtin::LtU16:       return cmp_bin_op_type(ctx, ctx.builtins.type_u16);
    case hir::expr::Builtin::EqU32:
    case hir::expr::Builtin::LtU32:       return cmp_bin_op_type(ctx, ctx.builtins.type_u32);
    case hir::expr::Builtin::EqU64:
    case hir::expr::Builtin::LtU64:       return cmp_bin_op_type(ctx, ctx.builtins.type_u64);
    case hir::expr::Builtin::EqF32:
    case hir::expr::Builtin::LtF32:       return cmp_bin_op_type(ctx, ctx.builtins.type_f32);
    case hir::expr::Builtin::EqF64:
    case hir::expr::Builtin::LtF64:       return cmp_bin_op_type(ctx, ctx.builtins.type_f64);
    case hir::expr::Builtin::EqBool:      return cmp_bin_op_type(ctx, ctx.builtins.type_bool);
    case hir::expr::Builtin::EqChar:      return cmp_bin_op_type(ctx, ctx.builtins.type_char);
    case hir::expr::Builtin::LogicAnd:
    case hir::expr::Builtin::LogicOr:     return cmp_bin_op_type(ctx, ctx.builtins.type_bool);
    case hir::expr::Builtin::LogicNot:    return id_op_type(ctx, ctx.builtins.type_bool);
    case hir::expr::Builtin::Abort:
    case hir::expr::Builtin::Todo:
    case hir::expr::Builtin::Unreachable: return ctx.builtins.type_never;
    }
    cpputil::unreachable();
}

auto ki::res::arith_bin_op_type(Context& ctx, hir::Type_id operand) -> hir::Type_id
{
    return ctx.arena.hir.types.push(
        hir::type::Function {
            .parameter_types = { operand, operand },
            .return_type     = operand,
        });
}

auto ki::res::cmp_bin_op_type(Context& ctx, hir::Type_id operand) -> hir::Type_id
{
    return ctx.arena.hir.types.push(
        hir::type::Function {
            .parameter_types = { operand, operand },
            .return_type     = ctx.builtins.type_bool,
        });
}

auto ki::res::id_op_type(Context& ctx, hir::Type_id operand) -> hir::Type_id
{
    return ctx.arena.hir.types.push(
        hir::type::Function {
            .parameter_types = { operand },
            .return_type     = operand,
        });
}

auto ki::res::fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag
{
    return hir::Template_parameter_tag { ++tags.current_template_parameter_tag };
}

auto ki::res::error_expression(Context const& ctx, lsp::Range range) -> hir::Expression
{
    return hir::Expression {
        .variant  = db::Error {},
        .type_id  = ctx.builtins.type_error,
        .mut_id   = ctx.builtins.mut_yes,
        .category = hir::Expression_category::Place,
        .range    = range,
    };
}

auto ki::res::unit_expression(Context const& ctx, lsp::Range range) -> hir::Expression
{
    return hir::Expression {
        .variant  = hir::expr::Tuple {},
        .type_id  = ctx.builtins.type_unit,
        .mut_id   = ctx.builtins.mut_no,
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

        ctx.add_diagnostic(std::move(warning));
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
            ctx.add_diagnostic(lsp::error(name.range, std::move(message)));
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
    -> hir::Type_id
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
    return type_id;
}

auto ki::res::fresh_integral_type_variable(Context& ctx, Block_state& state, lsp::Range origin)
    -> hir::Type_id
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
    return type_id;
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
                set_type_solution(db, ctx, state, data.var_id, hir::type::Builtin::I32);
            }
            else {
                auto message = std::format("Unsolved type variable: ?{}", data.var_id.get());
                ctx.add_diagnostic(lsp::error(data.origin, std::move(message)));
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
