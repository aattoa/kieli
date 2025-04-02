#include <libutl/utilities.hpp>
#include <libcompiler/cst/cst.hpp>

auto ki::cst::Path::head() const -> Path_segment const&
{
    cpputil::always_assert(not segments.empty());
    return segments.back();
}

auto ki::cst::Path::is_unqualified() const noexcept -> bool
{
    return std::holds_alternative<std::monostate>(root) and segments.size() == 1;
}
