#include <libutl/utilities.hpp>
#include <libutl/disjoint_set.hpp>
#include <libutl/index_vector.hpp>
#include <libutl/mailbox.hpp>
#include <libutl/string_pool.hpp>

auto utl::View::string(std::string_view string) const -> std::string_view
{
    return string.substr(offset, length);
}
