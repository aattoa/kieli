#include <libutl/utilities.hpp>
#include <libcompiler/ast/ast.hpp>

kieli::AST::AST(Module&& module) : module(std::make_unique<Module>(std::move(module))) {}

kieli::AST::AST(AST&&) noexcept = default;

auto kieli::AST::operator=(AST&&) noexcept -> AST& = default;

kieli::AST::~AST() = default;

auto kieli::ast::Path::is_simple_name() const noexcept -> bool
{
    return !root.has_value() && segments.empty();
}

auto kieli::AST::get() const -> Module const&
{
    cpputil::always_assert(module != nullptr);
    return *module;
}

auto kieli::AST::get() -> Module& // NOLINT(readability-make-member-function-const)
{
    cpputil::always_assert(module != nullptr);
    return *module;
}
