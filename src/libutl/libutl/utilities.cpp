#include <libutl/utilities.hpp>
#include <libutl/disjoint_set.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/mailbox.hpp>

utl::Safe_cast_argument_out_of_range::Safe_cast_argument_out_of_range()
    : invalid_argument { "utl::safe_cast argument out of target range" }
{}
