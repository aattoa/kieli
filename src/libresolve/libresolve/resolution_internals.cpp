#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::make_constants(hir::Arena& arenas) -> Constants
{
    return Constants {
        .i8_type          = arenas.types.push(hir::type::Integer::i8),
        .i16_type         = arenas.types.push(hir::type::Integer::i16),
        .i32_type         = arenas.types.push(hir::type::Integer::i32),
        .i64_type         = arenas.types.push(hir::type::Integer::i64),
        .u8_type          = arenas.types.push(hir::type::Integer::u8),
        .u16_type         = arenas.types.push(hir::type::Integer::u16),
        .u32_type         = arenas.types.push(hir::type::Integer::u32),
        .u64_type         = arenas.types.push(hir::type::Integer::u64),
        .boolean_type     = arenas.types.push(hir::type::Boolean {}),
        .floating_type    = arenas.types.push(hir::type::Floating {}),
        .string_type      = arenas.types.push(hir::type::String {}),
        .character_type   = arenas.types.push(hir::type::Character {}),
        .unit_type        = arenas.types.push(hir::type::Tuple {}),
        .error_type       = arenas.types.push(hir::Error {}),
        .mutability_yes   = arenas.mutabilities.push(kieli::Mutability::mut),
        .mutability_no    = arenas.mutabilities.push(kieli::Mutability::immut),
        .mutability_error = arenas.mutabilities.push(hir::Error {}),
    };
}

auto libresolve::Tag_state::fresh_template_parameter_tag() -> hir::Template_parameter_tag
{
    return hir::Template_parameter_tag { ++m_current_template_parameter_tag };
}

auto libresolve::Tag_state::fresh_local_variable_tag() -> hir::Local_variable_tag
{
    return hir::Local_variable_tag { ++m_current_local_variable_tag };
}

auto libresolve::error_type(Constants const& constants, kieli::Range const range) -> hir::Type
{
    return hir::Type { constants.error_type, range };
}

auto libresolve::error_expression(Constants const& constants, kieli::Range const range)
    -> hir::Expression
{
    return hir::Expression {
        hir::Error {},
        constants.error_type,
        hir::Expression_kind::place,
        range,
    };
}

auto libresolve::unit_expression(Constants const& constants, kieli::Range const range)
    -> hir::Expression
{
    return hir::Expression {
        hir::expression::Tuple {},
        constants.unit_type,
        hir::Expression_kind::value,
        range,
    };
}

auto libresolve::flatten_type(Context& context, Inference_state& state, hir::Type_variant& type)
    -> void
{
    if (auto const* const variable = std::get_if<hir::type::Variable>(&type)) {
        Type_variable_data& data = state.type_variables[variable->id];
        assert(variable->id == data.variable_id);
        if (data.is_solved) {
            type = context.hir.types[data.type_id];

            data.is_solved = true;
            return;
        }
        std::size_t const index = state.type_variable_disjoint_set.find(data.variable_id.get());
        if (data.variable_id.get() == index) {
            return;
        }
        Type_variable_data& representative_data = state.type_variables.underlying.at(index);
        hir::Type_variant&  representative_type = context.hir.types[representative_data.type_id];
        flatten_type(context, state, representative_type);
        if (representative_data.is_solved) {
            type           = representative_type;
            data.is_solved = true;
            return;
        }
    }
}

auto libresolve::set_solution(
    Context&            context,
    Inference_state&    state,
    Type_variable_data& variable_data,
    hir::Type_variant   solution) -> void
{
    auto const index = state.type_variable_disjoint_set.find(variable_data.variable_id.get());
    Type_variable_data& representative_data = state.type_variables.underlying.at(index);
    hir::Type_variant&  representative_type = context.hir.types[representative_data.type_id];
    if (representative_data.is_solved) {
        require_subtype_relationship(context, state, solution, representative_type);
    }
    representative_type           = std::move(solution);
    representative_data.is_solved = true;
}

auto libresolve::set_solution(
    Context&                  context,
    Inference_state&          state,
    Mutability_variable_data& variable_data,
    hir::Mutability_variant   solution) -> void
{
    auto const index = state.mutability_variable_disjoint_set.find(variable_data.variable_id.get());
    Mutability_variable_data& representative = state.mutability_variables.underlying.at(index);
    cpputil::always_assert(!std::exchange(representative.is_solved, true));
    context.hir.mutabilities[representative.mutability_id] = std::move(solution);
}

auto libresolve::fresh_general_type_variable(
    Inference_state& state, hir::Arena& arena, kieli::Range const origin) -> hir::Type
{
    auto const variable_id = hir::Type_variable_id { state.type_variables.size() };
    auto const type_id     = arena.types.push(hir::type::Variable { variable_id });
    state.type_variables.underlying.push_back(Type_variable_data {
        .kind        = hir::Type_variable_kind::general,
        .variable_id = variable_id,
        .type_id     = type_id,
        .origin      = origin,
    });
    (void)state.type_variable_disjoint_set.add();
    return hir::Type { type_id, origin };
}

auto libresolve::fresh_integral_type_variable(
    Inference_state& state, hir::Arena& arena, kieli::Range const origin) -> hir::Type
{
    auto const variable_id = hir::Type_variable_id { state.type_variables.size() };
    auto const type_id     = arena.types.push(hir::type::Variable { variable_id });
    state.type_variables.underlying.push_back(Type_variable_data {
        .kind        = hir::Type_variable_kind::integral,
        .variable_id = variable_id,
        .type_id     = type_id,
        .origin      = origin,
    });
    (void)state.type_variable_disjoint_set.add();
    return hir::Type { type_id, origin };
}

auto libresolve::fresh_mutability_variable(
    Inference_state& state, hir::Arena& arena, kieli::Range const origin) -> hir::Mutability
{
    auto const variable_id   = hir::Mutability_variable_id { state.mutability_variables.size() };
    auto const mutability_id = arena.mutabilities.push(hir::mutability::Variable { variable_id });
    state.mutability_variables.underlying.push_back(Mutability_variable_data {
        .variable_id   = variable_id,
        .mutability_id = mutability_id,
        .origin        = origin,
    });
    (void)state.mutability_variable_disjoint_set.add();
    return hir::Mutability { mutability_id, origin };
}

auto libresolve::ensure_no_unsolved_variables(Context& context, Inference_state& state) -> void
{
    for (Mutability_variable_data& data : state.mutability_variables.underlying) {
        if (!data.is_solved) {
            set_solution(context, state, data, kieli::Mutability::immut);
        }
    }
    for (Type_variable_data& data : state.type_variables.underlying) {
        flatten_type(context, state, context.hir.types[data.type_id]);
        if (!data.is_solved) {
            auto message = std::format("Unsolved type variable: ?{}", data.variable_id.get());
            kieli::add_error(context.db, state.document_id, data.origin, std::move(message));
            set_solution(context, state, data, hir::Error {});
        }
    }
}

auto libresolve::resolve_concept_reference(
    Context&            context,
    Inference_state&    state,
    hir::Scope_id       scope_id,
    hir::Environment_id environment_id,
    ast::Path const&    path) -> hir::Concept_id
{
    (void)context;
    (void)state;
    (void)scope_id;
    (void)environment_id;
    (void)path;
    cpputil::todo();
}
