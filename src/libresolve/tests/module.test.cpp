#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <cppunittest/unittest.hpp>

UNITTEST("libresolve resolve_import")
{
    kieli::Project_configuration const config { .root_directory = std::filesystem::current_path() };

    cpputil::mem::Stable_string_pool pool;

    auto name = [&](std::string_view const string) mutable {
        return kieli::Lower { kieli::Identifier { pool.add(string) }, kieli::Range::dummy() };
    };
    auto const import = [&](auto const... strings) {
        std::array const array { name(strings)... };
        return libresolve::resolve_import(config, array).value().module_path.string();
    };

    CHECK_EQUAL((config.root_directory / "a.kieli").string(), import("a"));
    CHECK_EQUAL((config.root_directory / "b" / "c.kieli").string(), import("b", "c"));
}
