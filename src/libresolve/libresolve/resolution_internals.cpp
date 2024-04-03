#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::Arenas::defaults() -> Arenas
{
    return Arenas {
        .info_arena        = Info_arena::with_default_page_size(),
        .environment_arena = Environment_arena::with_default_page_size(),
        .ast_node_arena    = ast::Node_arena::with_default_page_size(),
        .hir_node_arena    = hir::Node_arena::with_default_page_size(),
    };
}

auto libresolve::Arenas::type(hir::Type::Variant variant)
    -> utl::Mutable_wrapper<hir::Type::Variant>
{
    return hir_node_arena.wrap_mutable<hir::Type::Variant>(std::move(variant));
}

auto libresolve::Arenas::mutability(hir::Mutability::Variant variant)
    -> utl::Mutable_wrapper<hir::Mutability::Variant>
{
    return hir_node_arena.wrap_mutable<hir::Mutability::Variant>(std::move(variant));
}

auto libresolve::Arenas::wrap(hir::Expression expression) -> utl::Wrapper<hir::Expression>
{
    return hir_node_arena.wrap<hir::Expression>(std::move(expression));
}

auto libresolve::Arenas::wrap(hir::Pattern pattern) -> utl::Wrapper<hir::Pattern>
{
    return hir_node_arena.wrap<hir::Pattern>(std::move(pattern));
}

auto libresolve::Constants::make_with(Arenas& arenas) -> Constants
{
    return Constants {
        .i8_type          = arenas.type(kieli::built_in_type::Integer::i8),
        .i16_type         = arenas.type(kieli::built_in_type::Integer::i16),
        .i32_type         = arenas.type(kieli::built_in_type::Integer::i32),
        .i64_type         = arenas.type(kieli::built_in_type::Integer::i64),
        .u8_type          = arenas.type(kieli::built_in_type::Integer::u8),
        .u16_type         = arenas.type(kieli::built_in_type::Integer::u16),
        .u32_type         = arenas.type(kieli::built_in_type::Integer::u32),
        .u64_type         = arenas.type(kieli::built_in_type::Integer::u64),
        .boolean_type     = arenas.type(kieli::built_in_type::Boolean {}),
        .floating_type    = arenas.type(kieli::built_in_type::Floating {}),
        .string_type      = arenas.type(kieli::built_in_type::String {}),
        .character_type   = arenas.type(kieli::built_in_type::Character {}),
        .unit_type        = arenas.type(hir::type::Tuple {}),
        .error_type       = arenas.type(hir::type::Error {}),
        .mutability_yes   = arenas.mutability(hir::mutability::Concrete::mut),
        .mutability_no    = arenas.mutability(hir::mutability::Concrete::immut),
        .mutability_error = arenas.mutability(hir::mutability::Error {}),
    };
}

auto libresolve::error_expression(Constants const& constants, utl::Source_range const source_range)
    -> hir::Expression
{
    return hir::Expression {
        .variant      = hir::expression::Error {},
        .type         = error_type(constants, source_range),
        .source_range = source_range,
    };
}

auto libresolve::unit_expression(Constants const& constants, utl::Source_range const source_range)
    -> hir::Expression
{
    return hir::Expression {
        .variant      = hir::expression::Tuple {},
        .type         = unit_type(constants, source_range),
        .source_range = source_range,
    };
}

auto libresolve::error_type(Constants const& constants, utl::Source_range const source_range)
    -> hir::Type
{
    return hir::Type { constants.error_type, source_range };
}

auto libresolve::unit_type(Constants const& constants, utl::Source_range const source_range)
    -> hir::Type
{
    return hir::Type { constants.unit_type, source_range };
}

auto libresolve::Type_variable_data::solve_with(hir::Type::Variant solution) -> void
{
    cpputil::always_assert(!std::exchange(is_solved, true));
    variant.as_mutable() = std::move(solution);
}

auto libresolve::Mutability_variable_data::solve_with(hir::Mutability::Variant solution) -> void
{
    cpputil::always_assert(!std::exchange(is_solved, true));
    variant.as_mutable() = std::move(solution);
}

auto libresolve::Inference_state::fresh_general_type_variable(
    Arenas& arenas, utl::Source_range const origin) -> hir::Type
{
    auto const tag     = Type_variable_tag { type_variables.underlying.size() };
    auto const variant = arenas.type(hir::type::Variable { tag });
    type_variables.underlying.push_back(Type_variable_data {
        .tag     = tag,
        .kind    = hir::Type_variable_kind::general,
        .origin  = origin,
        .variant = variant,
    });
    return hir::Type { variant, origin };
}

auto libresolve::Inference_state::fresh_integral_type_variable(
    Arenas& arenas, utl::Source_range const origin) -> hir::Type
{
    auto const tag     = Type_variable_tag { type_variables.underlying.size() };
    auto const variant = arenas.type(hir::type::Variable { tag });
    type_variables.underlying.push_back(Type_variable_data {
        .tag     = tag,
        .kind    = hir::Type_variable_kind::integral,
        .origin  = origin,
        .variant = variant,
    });
    return hir::Type { variant, origin };
}

auto libresolve::Inference_state::fresh_mutability_variable(
    Arenas& arenas, utl::Source_range const origin) -> hir::Mutability
{
    auto const tag     = Mutability_variable_tag { mutability_variables.underlying.size() };
    auto const variant = arenas.mutability(hir::mutability::Variable { tag });
    mutability_variables.underlying.push_back(Mutability_variable_data {
        .tag     = tag,
        .origin  = origin,
        .variant = variant,
    });
    return hir::Mutability { variant, origin };
}

auto libresolve::Inference_state::ensure_no_unsolved_variables(kieli::Diagnostics& diagnostics)
    -> void
{
    for (Mutability_variable_data& data : mutability_variables.underlying) {
        if (!data.is_solved) {
            data.solve_with(hir::mutability::Concrete::immut);
        }
    }
    for (Type_variable_data& data : type_variables.underlying) {
        if (!data.is_solved) {
            diagnostics.emit(
                cppdiag::Severity::error,
                source,
                data.origin,
                "Unsolved type variable: ?{}",
                data.tag.get());
            data.solve_with(hir::type::Error {});
        }
    }
}
