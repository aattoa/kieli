#include <libutl/utilities.hpp>
#include <libutl/flatmap.hpp>
#include <cppunittest/unittest.hpp>

#define TEST(name) UNITTEST("libutl: " name)

TEST("Flatmap::add_or_assign")
{
    utl::Flatmap<std::string, int> map;
    REQUIRE(map.empty());
    REQUIRE(map.find("hello") == nullptr);

    map.add_or_assign("hello", 25);
    REQUIRE(map.size() == 1);
    REQUIRE(map.find("hello") != nullptr);
    REQUIRE(*map.find("hello") == 25);

    map.add_or_assign("hello", 100);
    REQUIRE(map.size() == 1);
    REQUIRE(map.find("hello") != nullptr);
    REQUIRE(*map.find("hello") == 100);

    map.add_or_assign("qwerty", 200);
    REQUIRE(map.size() == 2);
    REQUIRE(map.find("hello") != nullptr);
    REQUIRE(*map.find("hello") == 100);
    REQUIRE(map.find("qwerty") != nullptr);
    REQUIRE(*map.find("qwerty") == 200);
}
