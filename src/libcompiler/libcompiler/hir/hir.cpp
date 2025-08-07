#include <libcompiler/hir/hir.hpp>
#include <libcompiler/hir/formatters.hpp>

auto ki::hir::builtin_expr_name(expr::Builtin builtin) -> std::string_view
{
    switch (builtin) {
#define KIELI_X_BUILTIN_DO(identifier, spelling) \
    case expr::Builtin::identifier: return spelling;
#include <libcompiler/hir/builtin-x-macro-table.hpp>
#undef KIELI_X_BUILTIN_DO
    }
    cpputil::unreachable();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto ki::hir::get_builtin_expr(std::string_view name) -> std::optional<expr::Builtin>
{
#define KIELI_X_BUILTIN_DO(identifier, spelling) \
    if (name == (spelling))                      \
        return expr::Builtin::identifier;
#include <libcompiler/hir/builtin-x-macro-table.hpp>
#undef KIELI_X_BUILTIN_DO
    return std::nullopt;
}

auto ki::hir::builtin_type_name(type::Builtin builtin) -> std::string_view
{
    switch (builtin) {
    case type::Builtin::I8:     return "I8";
    case type::Builtin::I16:    return "I16";
    case type::Builtin::I32:    return "I32";
    case type::Builtin::I64:    return "I64";
    case type::Builtin::U8:     return "U8";
    case type::Builtin::U16:    return "U16";
    case type::Builtin::U32:    return "U32";
    case type::Builtin::U64:    return "U64";
    case type::Builtin::F32:    return "F32";
    case type::Builtin::F64:    return "F64";
    case type::Builtin::Bool:   return "Bool";
    case type::Builtin::Char:   return "Char";
    case type::Builtin::String: return "String";
    case type::Builtin::Never:  return "Never";
    }
    cpputil::unreachable();
}

auto ki::hir::get_builtin_type(std::string_view name) -> std::optional<type::Builtin>
{
    // clang-format off
    if (name == "I8")     return type::Builtin::I8;
    if (name == "I16")    return type::Builtin::I16;
    if (name == "I32")    return type::Builtin::I32;
    if (name == "I64")    return type::Builtin::I64;
    if (name == "U8")     return type::Builtin::U8;
    if (name == "U16")    return type::Builtin::U16;
    if (name == "U32")    return type::Builtin::U32;
    if (name == "U64")    return type::Builtin::U64;
    if (name == "F32")    return type::Builtin::F32;
    if (name == "F64")    return type::Builtin::F64;
    if (name == "Bool")   return type::Builtin::Bool;
    if (name == "Char")   return type::Builtin::Char;
    if (name == "String") return type::Builtin::String;
    if (name == "Never")  return type::Builtin::Never;
    // clang-format on
    return std::nullopt;
}

auto ki::hir::is_integral(type::Builtin builtin) -> bool
{
    switch (builtin) {
    case type::Builtin::I8:
    case type::Builtin::I16:
    case type::Builtin::I32:
    case type::Builtin::I64:
    case type::Builtin::U8:
    case type::Builtin::U16:
    case type::Builtin::U32:
    case type::Builtin::U64: return true;
    default:                 return false;
    }
}

auto ki::hir::describe_constructor(Constructor_body const& body) -> std::string_view
{
    auto const visitor = utl::Overload {
        [](hir::Unit_constructor const&) { return "unit"; },
        [](hir::Tuple_constructor const&) { return "tuple"; },
        [](hir::Struct_constructor const&) { return "struct"; },
    };
    return std::visit(visitor, body);
}

#define DEFINE_HIR_FORMAT_TO(...)                                     \
    void ki::hir::format_to(                                          \
        std::string&            output,                               \
        Arena const&            arena,                                \
        utl::String_pool const& pool,                                 \
        __VA_ARGS__ const&      object)                               \
    {                                                                 \
        dtl::With_arena<__VA_ARGS__> with_arena {                     \
            std::cref(pool),                                          \
            std::cref(arena),                                         \
            std::cref(object),                                        \
        };                                                            \
        std::format_to(std::back_inserter(output), "{}", with_arena); \
    }

DEFINE_HIR_FORMAT_TO(Expression);
DEFINE_HIR_FORMAT_TO(Expression_id);
DEFINE_HIR_FORMAT_TO(Pattern);
DEFINE_HIR_FORMAT_TO(Pattern_id);
DEFINE_HIR_FORMAT_TO(Type_id);
DEFINE_HIR_FORMAT_TO(Type_variant);
DEFINE_HIR_FORMAT_TO(Mutability);
DEFINE_HIR_FORMAT_TO(Mutability_id);
DEFINE_HIR_FORMAT_TO(Mutability_variant);

#undef DEFINE_HIR_FORMAT_TO
