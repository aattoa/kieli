#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libcompiler/ast/display.hpp>

auto ki::resolve::context(Database& db) -> Context
{
    auto hir       = hir::Arena {};
    auto constants = make_constants(hir);
    return Context {
        .db        = db,
        .hir       = std::move(hir),
        .info      = {},
        .tags      = {},
        .constants = constants,
        .documents = {},
    };
}

auto ki::resolve::make_constants(hir::Arena& arena) -> Constants
{
    return Constants {
        .type_i8        = arena.type.push(hir::type::Integer::I8),
        .type_i16       = arena.type.push(hir::type::Integer::I16),
        .type_i32       = arena.type.push(hir::type::Integer::I32),
        .type_i64       = arena.type.push(hir::type::Integer::I64),
        .type_u8        = arena.type.push(hir::type::Integer::U8),
        .type_u16       = arena.type.push(hir::type::Integer::U16),
        .type_u32       = arena.type.push(hir::type::Integer::U32),
        .type_u64       = arena.type.push(hir::type::Integer::U64),
        .type_boolean   = arena.type.push(hir::type::Boolean {}),
        .type_floating  = arena.type.push(hir::type::Floating {}),
        .type_string    = arena.type.push(hir::type::String {}),
        .type_character = arena.type.push(hir::type::Character {}),
        .type_unit      = arena.type.push(hir::type::Tuple {}),
        .type_error     = arena.type.push(Error {}),
        .mut_yes        = arena.mut.push(Mutability::Mut),
        .mut_no         = arena.mut.push(Mutability::Immut),
        .mut_error      = arena.mut.push(Error {}),
    };
}

auto ki::resolve::make_inference_state(Document_id doc_id) -> Inference_state
{
    return Inference_state {
        .type_vars    = {},
        .type_var_set = {},
        .mut_vars     = {},
        .mut_var_set  = {},
        .doc_id       = doc_id,
    };
}

auto ki::resolve::fresh_template_parameter_tag(Tags& tags) -> hir::Template_parameter_tag
{
    return hir::Template_parameter_tag { ++tags.current_template_parameter_tag };
}

auto ki::resolve::fresh_local_variable_tag(Tags& tags) -> hir::Local_variable_tag
{
    return hir::Local_variable_tag { ++tags.current_local_variable_tag };
}

auto ki::resolve::error_type(Constants const& constants, Range const range) -> hir::Type
{
    return hir::Type { .id = constants.type_error, .range = range };
}

auto ki::resolve::error_expression(Constants const& constants, Range const range) -> hir::Expression
{
    return hir::Expression {
        .variant  = Error {},
        .type     = constants.type_error,
        .category = hir::Expression_category::Place,
        .range    = range,
    };
}

auto ki::resolve::unit_expression(Constants const& constants, Range const range) -> hir::Expression
{
    return hir::Expression {
        .variant  = hir::expression::Tuple {},
        .type     = constants.type_unit,
        .category = hir::Expression_category::Value,
        .range    = range,
    };
}

void ki::resolve::flatten_type(Context& ctx, Inference_state& state, hir::Type_variant& type)
{
    if (auto const* const variable = std::get_if<hir::type::Variable>(&type)) {
        Type_variable_data& data = state.type_vars[variable->id];
        assert(variable->id == data.var_id);
        if (data.is_solved) {
            type = ctx.hir.type[data.type_id];

            data.is_solved = true;
            return;
        }
        std::size_t const index = state.type_var_set.find(data.var_id.get());
        if (data.var_id.get() == index) {
            return;
        }
        Type_variable_data& repr_data = state.type_vars.underlying.at(index);
        hir::Type_variant&  repr_type = ctx.hir.type[repr_data.type_id];
        flatten_type(ctx, state, repr_type);
        if (repr_data.is_solved) {
            type           = repr_type;
            data.is_solved = true;
            return;
        }
    }
}

void ki::resolve::set_solution(
    Context& ctx, Inference_state& state, Type_variable_data& data, hir::Type_variant solution)
{
    auto const          index     = state.type_var_set.find(data.var_id.get());
    Type_variable_data& repr_data = state.type_vars.underlying.at(index);
    hir::Type_variant&  repr_type = ctx.hir.type[repr_data.type_id];
    if (repr_data.is_solved) {
        require_subtype_relationship(ctx, state, data.origin, solution, repr_type);
    }
    repr_type           = std::move(solution);
    repr_data.is_solved = true;
}

void ki::resolve::set_solution(
    Context&                  ctx,
    Inference_state&          state,
    Mutability_variable_data& data,
    hir::Mutability_variant   solution)
{
    Mutability_variable_data& repr
        = state.mut_vars.underlying.at(state.mut_var_set.find(data.var_id.get()));
    cpputil::always_assert(not std::exchange(repr.is_solved, true));
    ctx.hir.mut[repr.mut_id] = std::move(solution);
}

auto ki::resolve::fresh_general_type_variable(
    Inference_state& state, hir::Arena& arena, Range const origin) -> hir::Type
{
    auto const var_id  = hir::Type_variable_id { state.type_vars.size() };
    auto const type_id = arena.type.push(hir::type::Variable { var_id });
    state.type_vars.underlying.push_back(Type_variable_data {
        .kind    = hir::Type_variable_kind::General,
        .var_id  = var_id,
        .type_id = type_id,
        .origin  = origin,
    });
    (void)state.type_var_set.add();
    return hir::Type { .id = type_id, .range = origin };
}

auto ki::resolve::fresh_integral_type_variable(
    Inference_state& state, hir::Arena& arena, Range const origin) -> hir::Type
{
    auto const var_id  = hir::Type_variable_id { state.type_vars.size() };
    auto const type_id = arena.type.push(hir::type::Variable { var_id });
    state.type_vars.underlying.push_back(Type_variable_data {
        .kind    = hir::Type_variable_kind::Integral,
        .var_id  = var_id,
        .type_id = type_id,
        .origin  = origin,
    });
    (void)state.type_var_set.add();
    return hir::Type { .id = type_id, .range = origin };
}

auto ki::resolve::fresh_mutability_variable(
    Inference_state& state, hir::Arena& arena, Range const origin) -> hir::Mutability
{
    auto const var_id = hir::Mutability_variable_id { state.mut_vars.size() };
    auto const mut_id = arena.mut.push(hir::mutability::Variable { var_id });
    state.mut_vars.underlying.push_back(Mutability_variable_data {
        .var_id = var_id,
        .mut_id = mut_id,
        .origin = origin,
    });
    (void)state.mut_var_set.add();
    return hir::Mutability { .id = mut_id, .range = origin };
}

void ki::resolve::ensure_no_unsolved_variables(Context& ctx, Inference_state& state)
{
    for (Mutability_variable_data& data : state.mut_vars.underlying) {
        if (not data.is_solved) {
            set_solution(ctx, state, data, Mutability::Immut);
        }
    }
    for (Type_variable_data& data : state.type_vars.underlying) {
        flatten_type(ctx, state, ctx.hir.type[data.type_id]);
        if (not data.is_solved) {
            auto message = std::format("Unsolved type variable: ?{}", data.var_id.get());
            add_error(ctx.db, state.doc_id, data.origin, std::move(message));
            set_solution(ctx, state, data, Error {});
        }
    }
}

auto ki::resolve::resolve_concept_reference(
    Context&            ctx,
    Inference_state&    state,
    hir::Scope_id       scope_id,
    hir::Environment_id env_id,
    ast::Path const&    path) -> hir::Concept_id
{
    (void)ctx;
    (void)state;
    (void)scope_id;
    (void)env_id;
    (void)path;
    cpputil::todo();
}

void ki::resolve::resolve_environment(Context& ctx, hir::Environment_id id)
{
    auto const visitor = utl::Overload {
        [&](hir::Function_id const id) { resolve_function_body(ctx, ctx.info.functions[id]); },
        [&](hir::Enumeration_id const id) { resolve_enumeration(ctx, ctx.info.enumerations[id]); },
        [&](hir::Concept_id const id) { resolve_concept(ctx, ctx.info.concepts[id]); },
        [&](hir::Alias_id const id) { resolve_alias(ctx, ctx.info.aliases[id]); },
        [&](hir::Module_id const id) { resolve_environment(ctx, ctx.info.modules[id].env_id); },
    };
    for (auto const& definition : ctx.info.environments[id].in_order) {
        std::visit(visitor, definition);
    }
}

void ki::resolve::debug_display_environment(Context const& ctx, hir::Environment_id const env_id)
{
    auto const& environment = ctx.info.environments[env_id];
    auto const& document    = ctx.documents.at(environment.doc_id);

    auto const visitor = utl::Overload {
        [&](hir::Function_id const& id) {
            auto const& function = ctx.info.functions[id];
            std::println(
                "function {}\nresolved: {}\nast: {}",
                ctx.db.string_pool.get(function.name.id),
                function.signature.has_value(),
                ast::display(document.ast, ctx.db.string_pool, function.ast));
        },
        [&](hir::Module_id const& id) {
            auto const& module = ctx.info.modules[id];
            std::println(
                "module {}\nast: {}",
                ctx.db.string_pool.get(module.name.id),
                ast::display(document.ast, ctx.db.string_pool, module.ast));
        },
        [&](hir::Enumeration_id const& id) {
            auto const& enumeration = ctx.info.enumerations[id];
            std::println(
                "enumeration {}\nresolved: {}\nast: {}",
                ctx.db.string_pool.get(enumeration.name.id),
                enumeration.hir.has_value(),
                ast::display(document.ast, ctx.db.string_pool, enumeration.ast));
        },
        [&](hir::Concept_id const& id) {
            auto const& concept_ = ctx.info.concepts[id];
            std::println(
                "concept {}\nresolved: {}\nast: {}",
                ctx.db.string_pool.get(concept_.name.id),
                concept_.hir.has_value(),
                ast::display(document.ast, ctx.db.string_pool, concept_.ast));
        },
        [&](hir::Alias_id const& id) {
            auto const& alias = ctx.info.aliases[id];
            std::println(
                "alias {}\nresolved: {}\nast: {}",
                ctx.db.string_pool.get(alias.name.id),
                alias.hir.has_value(),
                ast::display(document.ast, ctx.db.string_pool, alias.ast));
        },
    };

    for (auto const& definition : environment.in_order) {
        std::visit(visitor, definition);
    }
}
