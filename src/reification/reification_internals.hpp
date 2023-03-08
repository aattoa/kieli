#pragma once

#include "mir/mir.hpp"
#include "cir/cir.hpp"


namespace reification {

    // `wrap_type(x)` is shorthand for `utl::wrap(cir::Type::Variant { x })`
    inline auto wrap_type(cir::Type::Variant&& value) -> utl::Wrapper<cir::Type::Variant> {
        return utl::wrap(std::move(value));
    }

    class Context {
        utl::Wrapper<cir::Type::Variant> unit_type_value      = wrap_type(cir::type::Tuple {});
        utl::Wrapper<cir::Type::Variant> boolean_type_value   = wrap_type(cir::type::Boolean {});
        utl::Wrapper<cir::Type::Variant> string_type_value    = wrap_type(cir::type::String {});
        utl::Wrapper<cir::Type::Variant> character_type_value = wrap_type(cir::type::Character {});
        utl::Wrapper<cir::Type::Variant> i8_type_value        = wrap_type(cir::type::Integer::i8);
        utl::Wrapper<cir::Type::Variant> i16_type_value       = wrap_type(cir::type::Integer::i16);
        utl::Wrapper<cir::Type::Variant> i32_type_value       = wrap_type(cir::type::Integer::i32);
        utl::Wrapper<cir::Type::Variant> i64_type_value       = wrap_type(cir::type::Integer::i64);
        utl::Wrapper<cir::Type::Variant> u8_type_value        = wrap_type(cir::type::Integer::u8);
        utl::Wrapper<cir::Type::Variant> u16_type_value       = wrap_type(cir::type::Integer::u16);
        utl::Wrapper<cir::Type::Variant> u32_type_value       = wrap_type(cir::type::Integer::u32);
        utl::Wrapper<cir::Type::Variant> u64_type_value       = wrap_type(cir::type::Integer::u64);
        utl::Wrapper<cir::Type::Variant> floating_type_value  = wrap_type(cir::type::Floating {});
    public:
        auto reify_expression(mir::Expression const&) -> cir::Expression;
        auto reify_pattern   (mir::Pattern    const&) -> cir::Pattern;
        auto reify_type      (mir::Type             ) -> cir::Type;

        auto unit_type     (utl::Source_view) -> cir::Type;
        auto i8_type       (utl::Source_view) -> cir::Type;
        auto i16_type      (utl::Source_view) -> cir::Type;
        auto i32_type      (utl::Source_view) -> cir::Type;
        auto i64_type      (utl::Source_view) -> cir::Type;
        auto u8_type       (utl::Source_view) -> cir::Type;
        auto u16_type      (utl::Source_view) -> cir::Type;
        auto u32_type      (utl::Source_view) -> cir::Type;
        auto u64_type      (utl::Source_view) -> cir::Type;
        auto floating_type (utl::Source_view) -> cir::Type;
        auto character_type(utl::Source_view) -> cir::Type;
        auto boolean_type  (utl::Source_view) -> cir::Type;
        auto string_type   (utl::Source_view) -> cir::Type;
        auto size_type     (utl::Source_view) -> cir::Type;
    };

}