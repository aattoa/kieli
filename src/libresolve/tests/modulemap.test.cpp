#include <libutl/common/utilities.hpp>
#include <libresolve/module.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("libresolve " name, "[libresolve][module]") // NOLINT

namespace {
    auto test_project_path(std::string_view const path) -> std::filesystem::path
    {
        return (std::filesystem::current_path() / path).replace_extension(".kieli");
    }
    auto definition_count(libresolve::Module const& module) -> utl::Usize
    {
        return module.root_environment->lower_map.size()
             + module.root_environment->upper_map.size();
    }
} // namespace

TEST("read module map")
{
    kieli::Compile_info info;
    libresolve::Context context {
        .info_arena        = libresolve::Info_arena::with_page_size(16),
        .ast_node_arena    = ast::Node_arena::with_page_size(0),
        .environment_arena = utl::Wrapper_arena<libresolve::Environment>::with_page_size(16),
    };
    auto const module_map
        = libresolve::read_module_map(info, context, std::filesystem::current_path());
    REQUIRE(info.diagnostics.vector.empty());
    REQUIRE(module_map.size() == 3);
    REQUIRE(definition_count(module_map[test_project_path("main")]) == 1);
    REQUIRE(definition_count(module_map[test_project_path("b/c")]) == 2);
    REQUIRE(definition_count(module_map[test_project_path("a")]) == 3);
}
