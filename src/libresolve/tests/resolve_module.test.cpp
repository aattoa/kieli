#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libresolve: " name)

TEST("resolve_import")
{
    kieli::Project_configuration const config { .root_directory = std::filesystem::current_path() };

    auto name = [pool = utl::String_pool {}](std::string_view const string) mutable {
        return kieli::Name_lower {
            .identifier   = kieli::Identifier { pool.make(string) },
            .source_range = utl::Source_range { {}, {} },
        };
    };
    auto const import = [&](auto const... strings) {
        std::array const array { name(strings)... };
        return libresolve::resolve_import(config, array).value().module_path.string();
    };

    CHECK_EQUAL((config.root_directory / "a.kieli").string(), import("a"));
    CHECK_EQUAL((config.root_directory / "b" / "c.kieli").string(), import("b", "c"));
}
