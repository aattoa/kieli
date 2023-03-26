#include "utl/utilities.hpp"
#include "reification_internals.hpp"


auto reification::Context::unit_type     (utl::Source_view const view) -> cir::Type { return { unit_type_value,      0,                 view }; }
auto reification::Context::i8_type       (utl::Source_view const view) -> cir::Type { return { i8_type_value,        sizeof(utl::I8),    view }; }
auto reification::Context::i16_type      (utl::Source_view const view) -> cir::Type { return { i16_type_value,       sizeof(utl::I16),   view }; }
auto reification::Context::i32_type      (utl::Source_view const view) -> cir::Type { return { i32_type_value,       sizeof(utl::I32),   view }; }
auto reification::Context::i64_type      (utl::Source_view const view) -> cir::Type { return { i64_type_value,       sizeof(utl::I64),   view }; }
auto reification::Context::u8_type       (utl::Source_view const view) -> cir::Type { return { u8_type_value,        sizeof(utl::U8),    view }; }
auto reification::Context::u16_type      (utl::Source_view const view) -> cir::Type { return { u16_type_value,       sizeof(utl::U16),   view }; }
auto reification::Context::u32_type      (utl::Source_view const view) -> cir::Type { return { u32_type_value,       sizeof(utl::U32),   view }; }
auto reification::Context::u64_type      (utl::Source_view const view) -> cir::Type { return { u64_type_value,       sizeof(utl::U64),   view }; }
auto reification::Context::floating_type (utl::Source_view const view) -> cir::Type { return { floating_type_value,  sizeof(utl::Float), view }; }
auto reification::Context::boolean_type  (utl::Source_view const view) -> cir::Type { return { boolean_type_value,   sizeof(bool),      view }; }
auto reification::Context::character_type(utl::Source_view const view) -> cir::Type { return { character_type_value, sizeof(utl::Char),  view }; }
auto reification::Context::string_type   (utl::Source_view const view) -> cir::Type { return { string_type_value,    sizeof(void*) + sizeof(utl::Usize), view }; }
auto reification::Context::size_type     (utl::Source_view const view) -> cir::Type { return u64_type(view); }