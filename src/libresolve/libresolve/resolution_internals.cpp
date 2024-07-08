#include <libutl/safe_integer.hpp>
#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::Constants::make_with(hir::Arena& arenas) -> Constants
{
    return Constants {
        .i8_type          = arenas.types.push(kieli::built_in_type::Integer::i8),
        .i16_type         = arenas.types.push(kieli::built_in_type::Integer::i16),
        .i32_type         = arenas.types.push(kieli::built_in_type::Integer::i32),
        .i64_type         = arenas.types.push(kieli::built_in_type::Integer::i64),
        .u8_type          = arenas.types.push(kieli::built_in_type::Integer::u8),
        .u16_type         = arenas.types.push(kieli::built_in_type::Integer::u16),
        .u32_type         = arenas.types.push(kieli::built_in_type::Integer::u32),
        .u64_type         = arenas.types.push(kieli::built_in_type::Integer::u64),
        .boolean_type     = arenas.types.push(kieli::built_in_type::Boolean {}),
        .floating_type    = arenas.types.push(kieli::built_in_type::Floating {}),
        .string_type      = arenas.types.push(kieli::built_in_type::String {}),
        .character_type   = arenas.types.push(kieli::built_in_type::Character {}),
        .unit_type        = arenas.types.push(hir::type::Tuple {}),
        .error_type       = arenas.types.push(hir::type::Error {}),
        .mutability_yes   = arenas.mutabilities.push(hir::mutability::Concrete::mut),
        .mutability_no    = arenas.mutabilities.push(hir::mutability::Concrete::immut),
        .mutability_error = arenas.mutabilities.push(hir::mutability::Error {}),
    };
}

auto libresolve::Tag_state::fresh_template_parameter_tag() -> hir::Template_parameter_tag
{
    cpputil::always_assert(!utl::would_increment_overflow(m_current_template_parameter_tag));
    return hir::Template_parameter_tag { ++m_current_template_parameter_tag };
}

auto libresolve::Tag_state::fresh_local_variable_tag() -> hir::Local_variable_tag
{
    cpputil::always_assert(!utl::would_increment_overflow(m_current_local_variable_tag));
    return hir::Local_variable_tag { ++m_current_local_variable_tag };
}

auto libresolve::error_expression(Constants const& constants, kieli::Range const range)
    -> hir::Expression
{
    return hir::Expression {
        hir::expression::Error {},
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

auto libresolve::Inference_state::flatten(Context& context, hir::Type_variant& type) -> void
{
    if (auto const* const variable = std::get_if<hir::type::Variable>(&type)) {
        Type_variable_data& data = type_variables[variable->id];
        assert(variable->id == data.variable_id);
        if (data.is_solved) {
            type = context.hir.types[data.type_id];

            data.is_solved = true;
            return;
        }
        std::size_t const index = type_variable_disjoint_set.find(data.variable_id.get());
        if (data.variable_id.get() == index) {
            return;
        }
        Type_variable_data& representative_data = type_variables.underlying.at(index);
        hir::Type_variant&  representative_type = context.hir.types[representative_data.type_id];
        flatten(context, representative_type);
        if (representative_data.is_solved) {
            type           = representative_type;
            data.is_solved = true;
            return;
        }
    }
}

auto libresolve::Inference_state::set_solution(
    Context& context, Type_variable_data& variable_data, hir::Type_variant solution) -> void
{
    std::size_t const   index = type_variable_disjoint_set.find(variable_data.variable_id.get());
    Type_variable_data& representative_data = type_variables.underlying.at(index);
    hir::Type_variant&  representative_type = context.hir.types[representative_data.type_id];
    if (representative_data.is_solved) {
        require_subtype_relationship(context, *this, solution, representative_type);
    }
    representative_type           = std::move(solution);
    representative_data.is_solved = true;
}

auto libresolve::Inference_state::set_solution(
    Context&                  context,
    Mutability_variable_data& variable_data,
    hir::Mutability_variant   solution) -> void
{
    std::size_t const index
        = mutability_variable_disjoint_set.find(variable_data.variable_id.get());
    Mutability_variable_data& representative = mutability_variables.underlying.at(index);
    cpputil::always_assert(!std::exchange(representative.is_solved, true));
    context.hir.mutabilities[representative.mutability_id] = std::move(solution);
}

auto libresolve::Inference_state::fresh_general_type_variable(
    hir::Arena& arena, kieli::Range const origin) -> hir::Type
{
    auto const variable_id = hir::Type_variable_id { type_variables.size() };
    auto const type_id     = arena.types.push(hir::type::Variable { variable_id });
    type_variables.underlying.push_back(Type_variable_data {
        .kind        = hir::Type_variable_kind::general,
        .variable_id = variable_id,
        .type_id     = type_id,
        .origin      = origin,
    });
    (void)type_variable_disjoint_set.add();
    return hir::Type { type_id, origin };
}

auto libresolve::Inference_state::fresh_integral_type_variable(
    hir::Arena& arena, kieli::Range const origin) -> hir::Type
{
    auto const variable_id = hir::Type_variable_id { type_variables.size() };
    auto const type_id     = arena.types.push(hir::type::Variable { variable_id });
    type_variables.underlying.push_back(Type_variable_data {
        .kind        = hir::Type_variable_kind::integral,
        .variable_id = variable_id,
        .type_id     = type_id,
        .origin      = origin,
    });
    (void)type_variable_disjoint_set.add();
    return hir::Type { type_id, origin };
}

auto libresolve::Inference_state::fresh_mutability_variable(
    hir::Arena& arena, kieli::Range const origin) -> hir::Mutability
{
    auto const variable_id   = hir::Mutability_variable_id { mutability_variables.size() };
    auto const mutability_id = arena.mutabilities.push(hir::mutability::Variable { variable_id });
    mutability_variables.underlying.push_back(Mutability_variable_data {
        .variable_id   = variable_id,
        .mutability_id = mutability_id,
        .origin        = origin,
    });
    (void)mutability_variable_disjoint_set.add();
    return hir::Mutability { mutability_id, origin };
}

auto libresolve::ensure_no_unsolved_variables(Context& context, Inference_state& state) -> void
{
    for (Mutability_variable_data& data : state.mutability_variables.underlying) {
        if (!data.is_solved) {
            state.set_solution(context, data, hir::mutability::Concrete::immut);
        }
    }
    for (Type_variable_data& data : state.type_variables.underlying) {
        state.flatten(context, context.hir.types[data.type_id]);
        if (!data.is_solved) {
            kieli::emit_diagnostic(
                cppdiag::Severity::error,
                context.compile_info,
                state.source,
                data.origin,
                std::format("Unsolved type variable: ?{}", data.variable_id.get()));
            state.set_solution(context, data, hir::type::Error {});
        }
    }
}

auto libresolve::resolve_class_reference(
    Context&                    context,
    Inference_state&            state,
    Scope&                      scope,
    hir::Environment_id const   environment,
    ast::Class_reference const& class_reference) -> hir::Class_reference
{
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    (void)class_reference;
    cpputil::todo();
}
