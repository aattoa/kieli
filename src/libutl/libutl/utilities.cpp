#include <libutl/utilities.hpp>

#include <libutl/disjoint_set.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/mailbox.hpp>

utl::Safe_cast_argument_out_of_range::Safe_cast_argument_out_of_range()
    : invalid_argument { "utl::safe_cast argument out of target range" }
{}

auto utl::disable_short_string_optimization(std::string& string) -> void
{
    if (string.capacity() <= sizeof(std::string)) {
        string.reserve(sizeof(std::string) + 1);
    }
}
