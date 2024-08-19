#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>
#include <cppunittest/unittest.hpp>

namespace {
    struct Index : utl::Vector_index<Index> {
        using Vector_index::Vector_index;
    };

    struct Vector : utl::Index_vector<Index, std::string> {};

    template <class T>
    using Subscript = decltype(std::declval<T>()[Index(0UZ)]);

    static_assert(utl::vector_index<Index>);
    static_assert(std::three_way_comparable<Index>);
    static_assert(std::same_as<Subscript<Vector&&>, std::string&&>);
    static_assert(std::same_as<Subscript<Vector const&&>, std::string const&&>);
    static_assert(std::same_as<Subscript<Vector&>, std::string&>);
    static_assert(std::same_as<Subscript<Vector const&>, std::string const&>);
} // namespace

UNITTEST("libutl index_vector")
{
    Vector vector;

    auto const a = vector.push("hello, world");
    auto const b = vector.push(5, 'a');
    auto const c = vector.push("third");

    CHECK_EQUAL(vector[a], "hello, world");
    CHECK_EQUAL(vector[b], "aaaaa");
    CHECK_EQUAL(vector[c], "third");
}
