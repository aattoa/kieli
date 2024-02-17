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

auto libresolve::Constants::make_with(Arenas& arenas) -> Constants
{
    return Constants {
        .i8_type        = arenas.type(kieli::built_in_type::Integer::i8),
        .i16_type       = arenas.type(kieli::built_in_type::Integer::i16),
        .i32_type       = arenas.type(kieli::built_in_type::Integer::i32),
        .i64_type       = arenas.type(kieli::built_in_type::Integer::i64),
        .u8_type        = arenas.type(kieli::built_in_type::Integer::u8),
        .u16_type       = arenas.type(kieli::built_in_type::Integer::u16),
        .u32_type       = arenas.type(kieli::built_in_type::Integer::u32),
        .u64_type       = arenas.type(kieli::built_in_type::Integer::u64),
        .boolean_type   = arenas.type(kieli::built_in_type::Boolean {}),
        .floating_type  = arenas.type(kieli::built_in_type::Floating {}),
        .string_type    = arenas.type(kieli::built_in_type::String {}),
        .character_type = arenas.type(kieli::built_in_type::Character {}),
        .unit_type      = arenas.type(hir::type::Tuple {}),
        .error_type     = arenas.type(hir::type::Error {}),
        .mutability     = arenas.mutability(hir::Mutability::Concrete { .is_mutable = true }),
        .immutability   = arenas.mutability(hir::Mutability::Concrete { .is_mutable = false }),
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

auto libresolve::error_type(Constants const& constants, utl::Source_range const source_range)
    -> hir::Type
{
    return hir::Type {
        .variant      = constants.error_type,
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

auto libresolve::unit_type(Constants const& constants, utl::Source_range const source_range)
    -> hir::Type
{
    return hir::Type {
        .variant      = constants.unit_type,
        .source_range = source_range,
    };
}
