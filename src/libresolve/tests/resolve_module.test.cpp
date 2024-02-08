#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libresolve: " name)

TEST("resolve_import")
{
    auto const path = std::filesystem::current_path();

    auto name = [pool = utl::String_pool {}](std::string_view const string) mutable {
        return kieli::Name_lower {
            .identifier   = kieli::Identifier { pool.make(string) },
            .source_range = utl::Source_range { {}, {} },
        };
    };
    auto const import = [&](auto const& array) {
        return libresolve::resolve_import(path, array).value().module_path.string();
    };

    CHECK_EQUAL((path / "a.kieli").string(), import(std::array { name("a") }));
    CHECK_EQUAL((path / "b" / "c.kieli").string(), import(std::array { name("b"), name("c") }));
}
