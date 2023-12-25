#include <libutl/common/utilities.hpp>
#include <libresolve/module.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("libresolve " name, "[libresolve][module]") // NOLINT

namespace {
    auto test_project_path(std::string_view const path) -> std::filesystem::path
    {
        return (std::filesystem::current_path() / path).replace_extension(".kieli");
    }
    auto definition_count(libresolve::Module_info const& module_info) -> utl::Usize
    {
        return std::get<ast::Module>(module_info.variant).definitions.size();
    }
} // namespace

TEST("read module map")
{
    kieli::Compile_info info;
    auto const module_map = libresolve::read_module_map(info, std::filesystem::current_path());
    REQUIRE(info.diagnostics.vector.empty());
    REQUIRE(module_map.size() == 3);
    REQUIRE(definition_count(module_map[test_project_path("main")]) == 1);
    REQUIRE(definition_count(module_map[test_project_path("b/c")]) == 2);
    REQUIRE(definition_count(module_map[test_project_path("a")]) == 3);
}
