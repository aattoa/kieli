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

    auto mock_resolution_arenas() -> libresolve::Arenas
    {
        return libresolve::Arenas {
            .info_arena        = libresolve::Info_arena::with_page_size(16),
            .environment_arena = libresolve::Environment_arena::with_page_size(16),
            .ast_node_arena    = ast::Node_arena::with_page_size(16),
        };
    }
} // namespace

TEST("resolve import path")
{
    auto const path = std::filesystem::current_path();

    auto name = [pool = utl::String_pool {}](std::string_view const string) mutable {
        return kieli::Name_lower {
            .identifier   = kieli::Identifier { pool.make(string) },
            .source_range = utl::Source_range { {}, {} },
        };
    };
    auto const import = [&](auto const& array) {
        return libresolve::resolve_import_path(path, array).value().string();
    };

    CHECK_EQUAL((path / "a.kieli").string(), import(std::array { name("a") }));
    CHECK_EQUAL((path / "b" / "c.kieli").string(), import(std::array { name("b"), name("c") }));
}

TEST("read module map")
{
    auto       arenas = mock_resolution_arenas();
    auto       info   = kieli::Compile_info {};
    auto const path   = std::filesystem::current_path();

    try {
        auto const module_map = libresolve::read_module_map(arenas, info, path);
        REQUIRE_EQUAL(info.diagnostics.format_all(cppdiag::Colors::none()), "");
        REQUIRE_EQUAL(module_map.size(), 3UZ);
        CHECK_EQUAL(definition_count(module_map[test_project_path("main")]), 1UZ);
        CHECK_EQUAL(definition_count(module_map[test_project_path("b/c")]), 2UZ);
        CHECK_EQUAL(definition_count(module_map[test_project_path("a")]), 3UZ);
    }
    catch (kieli::Compilation_failure const&) {
        std::println(stderr, "{}", info.diagnostics.format_all(cppdiag::Colors::none()));
        REQUIRE(false);
    }
}
