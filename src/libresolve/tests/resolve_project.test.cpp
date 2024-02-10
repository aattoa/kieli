#include <libutl/common/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libresolve: " name)

TEST("resolve_project")
{
    (void)kieli::resolve_project(kieli::Project_configuration {
        .root_directory = std::filesystem::current_path(),
    });
}
