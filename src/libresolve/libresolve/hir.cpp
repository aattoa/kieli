#include <libresolve/hir.hpp>
#include <libresolve/hir_formatters.hpp>

#define DEFINE_HIR_FORMAT_TO(...)                                                           \
    auto libresolve::hir::format_to(__VA_ARGS__ const& object, std::string& output) -> void \
    {                                                                                       \
        std::format_to(std::back_inserter(output), "{}", object);                           \
    }

DEFINE_HIR_FORMAT_TO(Expression);
DEFINE_HIR_FORMAT_TO(Pattern);
DEFINE_HIR_FORMAT_TO(Type);
DEFINE_HIR_FORMAT_TO(Mutability);

#undef DEFINE_HIR_FORMAT_TO
