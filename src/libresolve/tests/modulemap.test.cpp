#include <libutl/common/utilities.hpp>
#include <libresolve/module.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libresolve: " name)

namespace {
    auto test_project_path(std::string_view const path) -> std::filesystem::path
    {
        return (std::filesystem::current_path() / path).replace_extension(".kieli");
    }

    auto definition_count(libresolve::Module const& module) -> std::size_t
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
        .compile_info      = info,
    };
    auto const module_map = libresolve::read_module_map(context, std::filesystem::current_path());
    REQUIRE_EQUAL(info.diagnostics.format_all({}), "");
    REQUIRE_EQUAL(module_map.size(), 3UZ);
    CHECK_EQUAL(definition_count(module_map[test_project_path("main")]), 1UZ);
    CHECK_EQUAL(definition_count(module_map[test_project_path("b/c")]), 2UZ);
    CHECK_EQUAL(definition_count(module_map[test_project_path("a")]), 3UZ);
}
