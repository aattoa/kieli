#include <libutl/common/utilities.hpp>
#include <libutl/diag/internal/diag.hpp>
#include <catch2/catch_test_macros.hpp>

#define TEST(name) TEST_CASE("libutl-diag internal " name, "[libutl][diag]")

TEST("get_relevant_lines")
{
    using Lines = std::vector<std::string_view>;
    REQUIRE(
        utl::diag::internal::get_relevant_lines("hello, world!", { 1, 2 }, { 1, 5 })
        == Lines { "hello, world!" });
    REQUIRE(
        utl::diag::internal::get_relevant_lines("aaa\nbbb\nccc", { 1, 1 }, { 1, 2 })
        == Lines { "aaa" });
    REQUIRE(
        utl::diag::internal::get_relevant_lines("aaa\nbbb\nccc", { 2, 1 }, { 2, 2 })
        == Lines { "bbb" });
    REQUIRE(
        utl::diag::internal::get_relevant_lines("aaa\nbbb\nccc", { 3, 1 }, { 3, 2 })
        == Lines { "ccc" });
    REQUIRE(
        utl::diag::internal::get_relevant_lines("aaa\nbbb\nccc\nddd\neee", { 1, 1 }, { 5, 2 })
        == Lines { "aaa", "bbb", "ccc", "ddd", "eee" });
    REQUIRE(
        utl::diag::internal::get_relevant_lines("aaa\nbbb\nccc\nddd\neee", { 1, 1 }, { 3, 2 })
        == Lines { "aaa", "bbb", "ccc" });
    REQUIRE(
        utl::diag::internal::get_relevant_lines("aaa\nbbb\nccc\nddd\neee", { 3, 1 }, { 5, 2 })
        == Lines { "ccc", "ddd", "eee" });
}
