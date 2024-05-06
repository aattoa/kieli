#include <libutl/utilities.hpp>
#include <libutl/index_vector.hpp>

namespace {

    using Index  = utl::Vector_index<struct Test_index_tag>;
    using Vector = utl::Index_vector<Index, std::string>;

    template <class T>
    using Subscript = decltype(std::declval<T>()[Index(0)]);

    static_assert(utl::vector_index<Index>);
    static_assert(std::three_way_comparable<Index>);
    static_assert(std::three_way_comparable<Vector>);
    static_assert(std::is_same_v<Subscript<Vector&&>, std::string&&>);
    static_assert(std::is_same_v<Subscript<Vector const&&>, std::string const&&>);
    static_assert(std::is_same_v<Subscript<Vector&>, std::string&>);
    static_assert(std::is_same_v<Subscript<Vector const&>, std::string const&>);

} // namespace
