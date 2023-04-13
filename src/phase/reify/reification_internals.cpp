#include "utl/utilities.hpp"
#include "reification_internals.hpp"


reification::Reification_constants::Reification_constants(cir::Node_arena& arena)
    : unit_type      { arena.wrap<cir::Type::Variant>(cir::type::Tuple {}) }
    , boolean_type   { arena.wrap<cir::Type::Variant>(cir::type::Boolean {}) }
    , string_type    { arena.wrap<cir::Type::Variant>(cir::type::String {}) }
    , character_type { arena.wrap<cir::Type::Variant>(cir::type::Character {}) }
    , i8_type        { arena.wrap<cir::Type::Variant>(cir::type::Integer::i8) }
    , i16_type       { arena.wrap<cir::Type::Variant>(cir::type::Integer::i16) }
    , i32_type       { arena.wrap<cir::Type::Variant>(cir::type::Integer::i32) }
    , i64_type       { arena.wrap<cir::Type::Variant>(cir::type::Integer::i64) }
    , u8_type        { arena.wrap<cir::Type::Variant>(cir::type::Integer::u8) }
    , u16_type       { arena.wrap<cir::Type::Variant>(cir::type::Integer::u16) }
    , u32_type       { arena.wrap<cir::Type::Variant>(cir::type::Integer::u32) }
    , u64_type       { arena.wrap<cir::Type::Variant>(cir::type::Integer::u64) }
    , floating_type  { arena.wrap<cir::Type::Variant>(cir::type::Floating {}) } {}


auto reification::Context::unit_type     (utl::Source_view const view) -> cir::Type { return { constants.unit_type,      0,                  view }; }
auto reification::Context::i8_type       (utl::Source_view const view) -> cir::Type { return { constants.i8_type,        sizeof(utl::I8),    view }; }
auto reification::Context::i16_type      (utl::Source_view const view) -> cir::Type { return { constants.i16_type,       sizeof(utl::I16),   view }; }
auto reification::Context::i32_type      (utl::Source_view const view) -> cir::Type { return { constants.i32_type,       sizeof(utl::I32),   view }; }
auto reification::Context::i64_type      (utl::Source_view const view) -> cir::Type { return { constants.i64_type,       sizeof(utl::I64),   view }; }
auto reification::Context::u8_type       (utl::Source_view const view) -> cir::Type { return { constants.u8_type,        sizeof(utl::U8),    view }; }
auto reification::Context::u16_type      (utl::Source_view const view) -> cir::Type { return { constants.u16_type,       sizeof(utl::U16),   view }; }
auto reification::Context::u32_type      (utl::Source_view const view) -> cir::Type { return { constants.u32_type,       sizeof(utl::U32),   view }; }
auto reification::Context::u64_type      (utl::Source_view const view) -> cir::Type { return { constants.u64_type,       sizeof(utl::U64),   view }; }
auto reification::Context::floating_type (utl::Source_view const view) -> cir::Type { return { constants.floating_type,  sizeof(utl::Float), view }; }
auto reification::Context::boolean_type  (utl::Source_view const view) -> cir::Type { return { constants.boolean_type,   sizeof(bool),       view }; }
auto reification::Context::character_type(utl::Source_view const view) -> cir::Type { return { constants.character_type, sizeof(utl::Char),  view }; }
auto reification::Context::string_type   (utl::Source_view const view) -> cir::Type { return { constants.string_type,    sizeof(void*) + sizeof(utl::Usize), view }; }
auto reification::Context::size_type     (utl::Source_view const view) -> cir::Type { return u64_type(view); }


auto reification::Context::error(
    utl::Source_view                    const source_view,
    utl::diagnostics::Message_arguments const message_arguments) -> void
{
    compilation_info.diagnostics.emit_simple_error(message_arguments.add_source_info(source, source_view));
}