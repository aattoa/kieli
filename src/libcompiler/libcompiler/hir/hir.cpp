#include <libcompiler/hir/hir.hpp>
#include <libcompiler/hir/formatters.hpp>

auto ki::hir::integer_name(type::Integer const type) -> std::string_view
{
    switch (type) {
    case type::Integer::I8:  return "I8";
    case type::Integer::I16: return "I16";
    case type::Integer::I32: return "I32";
    case type::Integer::I64: return "I64";
    case type::Integer::U8:  return "U8";
    case type::Integer::U16: return "U16";
    case type::Integer::U32: return "U32";
    case type::Integer::U64: return "U64";
    default:                 cpputil::unreachable();
    }
}

auto ki::hir::expression_type(Expression const& expression) -> Type
{
    return Type { .id = expression.type, .range = expression.range };
}

auto ki::hir::pattern_type(Pattern const& pattern) -> Type
{
    return Type { .id = pattern.type, .range = pattern.range };
}

#define DEFINE_HIR_FORMAT_TO(...)                                     \
    void ki::hir::format(                                             \
        Arena const&            arena,                                \
        utl::String_pool const& pool,                                 \
        __VA_ARGS__ const&      object,                               \
        std::string&            output)                               \
    {                                                                 \
        dtl::With_arena<__VA_ARGS__> with_arena {                     \
            std::cref(pool),                                          \
            std::cref(arena),                                         \
            std::cref(object),                                        \
        };                                                            \
        std::format_to(std::back_inserter(output), "{}", with_arena); \
    }

DEFINE_HIR_FORMAT_TO(Expression);
DEFINE_HIR_FORMAT_TO(Pattern);
DEFINE_HIR_FORMAT_TO(Type);
DEFINE_HIR_FORMAT_TO(Type_variant);
DEFINE_HIR_FORMAT_TO(Mutability);
DEFINE_HIR_FORMAT_TO(Mutability_variant);

#undef DEFINE_HIR_FORMAT_TO
