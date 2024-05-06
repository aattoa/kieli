#include <libutl/utilities.hpp>

#include <libutl/disjoint_set.hpp>
#include <libutl/flatmap.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/pooled_string.hpp>
#include <libutl/safe_integer.hpp>
#include <libutl/source.hpp>
#include <libutl/timer.hpp>
#include <libutl/wrapper.hpp>

utl::Safe_cast_argument_out_of_range::Safe_cast_argument_out_of_range()
    : invalid_argument { "utl::safe_cast argument out of target range" }
{}

auto utl::disable_short_string_optimization(std::string& string) -> void
{
    if (string.capacity() <= sizeof(std::string)) {
        string.reserve(sizeof(std::string) + 1);
    }
}

auto utl::Relative_string::view_in(std::string_view const string) const -> std::string_view
{
    cpputil::always_assert(string.size() >= (offset + length));
    return string.substr(offset, length);
}
