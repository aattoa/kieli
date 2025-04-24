#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libcompiler/ast/display.hpp>

auto ki::res::context(db::Document_id doc_id) -> Context
{
    auto hir       = hir::Arena {};
    auto constants = make_constants(hir);
    return Context {
        .hir       = std::move(hir),
        .tags      = {},
        .constants = constants,
        .scopes    = {},
        .scope_map = {},
        .doc_id    = doc_id,
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

auto ki::res::error_type(Constants const& constants, lsp::Range range) -> hir::Type
{
    return hir::Type { .id = constants.type_error, .range = range };
}

auto ki::res::error_expression(Constants const& constants, lsp::Range range) -> hir::Expression
{
    return hir::Expression {
        .variant  = db::Error {},
        .type     = constants.type_error,
        .category = hir::Expression_category::Place,
        .range    = range,
    };
}

auto ki::res::unit_expression(Constants const& constants, lsp::Range range) -> hir::Expression
{
    return hir::Expression {
        .variant  = hir::expr::Tuple {},
        .type     = constants.type_unit,
        .category = hir::Expression_category::Value,
        .range    = range,
    };
}

void ki::res::flatten_type(Context& ctx, Inference_state& state, hir::Type_variant& type)
{
    if (auto const* const variable = std::get_if<hir::type::Variable>(&type)) {
        Type_variable_data& data = state.type_vars[variable->id];
        assert(variable->id == data.var_id);
        if (data.is_solved) {
            type = ctx.hir.types[data.type_id];

            data.is_solved = true;
            return;
        }
        std::size_t const index = state.type_var_set.find(data.var_id.get());
        if (data.var_id.get() == index) {
            return;
        }
        Type_variable_data& repr_data = state.type_vars.underlying.at(index);
        hir::Type_variant&  repr_type = ctx.hir.types[repr_data.type_id];
        flatten_type(ctx, state, repr_type);
        if (repr_data.is_solved) {
            type           = repr_type;
            data.is_solved = true;
            return;
        }
    }
}

void ki::res::set_type_solution(
    db::Database&       db,
    Context&            ctx,
    Inference_state&    state,
    Type_variable_data& data,
    hir::Type_variant   solution)
{
    auto& repr_data = state.type_vars.underlying.at(state.type_var_set.find(data.var_id.get()));
    auto& repr_type = ctx.hir.types[repr_data.type_id];
    if (repr_data.is_solved) {
        require_subtype_relationship(db, ctx, state, data.origin, solution, repr_type);
    }
    repr_type           = std::move(solution);
    repr_data.is_solved = true;
}

void ki::res::set_mut_solution(
    Context&                  ctx,
    Inference_state&          state,
    Mutability_variable_data& data,
    hir::Mutability_variant   solution)
{
    auto& repr = state.mut_vars.underlying.at(state.mut_var_set.find(data.var_id.get()));
    cpputil::always_assert(not std::exchange(repr.is_solved, true));
    ctx.hir.mutabilities[repr.mut_id] = std::move(solution);
}

auto ki::res::fresh_general_type_variable(
    Inference_state& state, hir::Arena& arena, lsp::Range origin) -> hir::Type
{
    auto const var_id  = hir::Type_variable_id { state.type_vars.size() };
    auto const type_id = arena.types.push(hir::type::Variable { var_id });
    state.type_vars.underlying.push_back(Type_variable_data {
        .kind    = hir::Type_variable_kind::General,
        .var_id  = var_id,
        .type_id = type_id,
        .origin  = origin,
    });
    (void)state.type_var_set.add();
    return hir::Type { .id = type_id, .range = origin };
}

auto ki::res::fresh_integral_type_variable(
    Inference_state& state, hir::Arena& arena, lsp::Range origin) -> hir::Type
{
    auto const var_id  = hir::Type_variable_id { state.type_vars.size() };
    auto const type_id = arena.types.push(hir::type::Variable { var_id });
    state.type_vars.underlying.push_back(Type_variable_data {
        .kind    = hir::Type_variable_kind::Integral,
        .var_id  = var_id,
        .type_id = type_id,
        .origin  = origin,
    });
    (void)state.type_var_set.add();
    return hir::Type { .id = type_id, .range = origin };
}

auto ki::res::fresh_mutability_variable(
    Inference_state& state, hir::Arena& arena, lsp::Range origin) -> hir::Mutability
{
    auto const var_id = hir::Mutability_variable_id { state.mut_vars.size() };
    auto const mut_id = arena.mutabilities.push(hir::mut::Variable { var_id });
    state.mut_vars.underlying.push_back(Mutability_variable_data {
        .var_id = var_id,
        .mut_id = mut_id,
        .origin = origin,
    });
    (void)state.mut_var_set.add();
    return hir::Mutability { .id = mut_id, .range = origin };
}

void ki::res::ensure_no_unsolved_variables(db::Database& db, Context& ctx, Inference_state& state)
{
    for (Mutability_variable_data& data : state.mut_vars.underlying) {
        if (not data.is_solved) {
            set_mut_solution(ctx, state, data, db::Mutability::Immut);
        }
    }
    for (Type_variable_data& data : state.type_vars.underlying) {
        flatten_type(ctx, state, ctx.hir.types[data.type_id]);
        if (not data.is_solved) {
            if (data.kind == hir::Type_variable_kind::Integral) {
                set_type_solution(db, ctx, state, data, hir::type::Integer::I32);
            }
            else {
                auto message = std::format("Unsolved type variable: ?{}", data.var_id.get());
                db::add_error(db, ctx.doc_id, data.origin, std::move(message));
                set_type_solution(db, ctx, state, data, db::Error {});
            }
        }
    }
}

void ki::res::resolve_environment(db::Database& db, Context& ctx, hir::Environment_id id)
{
    auto const visitor = utl::Overload {
        [&](hir::Function_id id) { resolve_function_body(db, ctx, id); },
        [&](hir::Enumeration_id id) { resolve_enumeration(db, ctx, id); },
        [&](hir::Concept_id id) { resolve_concept(db, ctx, id); },
        [&](hir::Alias_id id) { resolve_alias(db, ctx, id); },
        [&](hir::Module_id id) { resolve_environment(db, ctx, ctx.hir.modules[id].mod_env_id); },
        [](auto) { cpputil::unreachable(); },
    };
    for (auto const& definition : ctx.hir.environments[id].in_order) {
        std::visit(visitor, definition);
    }
}

void ki::res::debug_display_environment(
    db::Database const& db, Context const& ctx, hir::Environment_id const env_id)
{
    auto const& env = ctx.hir.environments[env_id];
    auto const& ast = db.documents[env.doc_id].arena.ast;

    auto const visitor = utl::Overload {
        [&](hir::Function_id const& id) {
            auto const& function = ctx.hir.functions[id];
            std::println(
                "function {}\nresolved: {}\nast: {}",
                db.string_pool.get(function.name.id),
                function.signature.has_value(),
                ast::display(ast, db.string_pool, function.ast));
        },
        [&](hir::Module_id const& id) {
            auto const& module = ctx.hir.modules[id];
            std::println("module {}", db.string_pool.get(module.name.id));
        },
        [&](hir::Enumeration_id const& id) {
            auto const& enumeration = ctx.hir.enumerations[id];
            std::println(
                "enumeration {}\nresolved: {}\nast: {}",
                db.string_pool.get(enumeration.name.id),
                enumeration.hir.has_value(),
                ast::display(ast, db.string_pool, enumeration.ast));
        },
        [&](hir::Concept_id const& id) {
            auto const& concept_ = ctx.hir.concepts[id];
            std::println(
                "concept {}\nresolved: {}\nast: {}",
                db.string_pool.get(concept_.name.id),
                concept_.hir.has_value(),
                ast::display(ast, db.string_pool, concept_.ast));
        },
        [&](hir::Alias_id const& id) {
            auto const& alias = ctx.hir.aliases[id];
            std::println(
                "alias {}\nresolved: {}\nast: {}",
                db.string_pool.get(alias.name.id),
                alias.hir.has_value(),
                ast::display(ast, db.string_pool, alias.ast));
        },
        [](auto) { cpputil::unreachable(); },
    };

    for (auto const& definition : env.in_order) {
        std::visit(visitor, definition);
    }
}
