#include <libcompiler/hir/hir.hpp>
#include <libcompiler/hir/formatters.hpp>

auto hir::expression_type(Expression const& expression) -> Type
{
    return Type { expression.type, expression.range };
}

auto hir::pattern_type(Pattern const& pattern) -> Type
{
    return Type { pattern.type, pattern.range };
}

#define DEFINE_HIR_FORMAT_TO(...)                                                                \
    auto hir::format(Arena const& arena, __VA_ARGS__ const& object, std::string& output) -> void \
    {                                                                                            \
        std::format_to(                                                                          \
            std::back_inserter(output),                                                          \
            "{}",                                                                                \
            hir::dtl::With_arena<__VA_ARGS__> { std::cref(arena), std::cref(object) });          \
    }

DEFINE_HIR_FORMAT_TO(Expression);
DEFINE_HIR_FORMAT_TO(Pattern);
DEFINE_HIR_FORMAT_TO(Type);
DEFINE_HIR_FORMAT_TO(Type_variant);
DEFINE_HIR_FORMAT_TO(Mutability);
DEFINE_HIR_FORMAT_TO(Mutability_variant);

#undef DEFINE_HIR_FORMAT_TO
