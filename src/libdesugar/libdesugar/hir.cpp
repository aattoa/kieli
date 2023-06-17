#include <libutl/common/utilities.hpp>
#include <libdesugar/hir.hpp>


auto hir::Qualified_name::is_upper() const noexcept -> bool {
    return primary_name.is_upper.get();
}
auto hir::Qualified_name::is_unqualified() const noexcept -> bool {
    return !root_qualifier.has_value() && middle_qualifiers.empty();
}
