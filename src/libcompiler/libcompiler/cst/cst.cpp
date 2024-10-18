#include <libutl/utilities.hpp>
#include <libcompiler/cst/cst.hpp>

auto kieli::cst::Path::head() const -> Path_segment const&
{
    cpputil::always_assert(not segments.empty());
    return segments.back();
}

auto kieli::cst::Path::is_upper() const -> bool
{
    return head().name.is_upper();
}

auto kieli::cst::Path::is_unqualified() const noexcept -> bool
{
    return std::holds_alternative<std::monostate>(root) and segments.size() == 1;
}

kieli::CST::CST(Module&& module) : module(std::make_unique<Module>(std::move(module))) {}

kieli::CST::CST(CST&&) noexcept = default;

auto kieli::CST::operator=(CST&&) noexcept -> CST& = default;

kieli::CST::~CST() = default;

auto kieli::CST::get() const -> Module const&
{
    cpputil::always_assert(module != nullptr);
    return *module;
}

auto kieli::CST::get() -> Module& // NOLINT(readability-make-member-function-const)
{
    cpputil::always_assert(module != nullptr);
    return *module;
}
