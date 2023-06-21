#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>


auto libformat::State::newline() noexcept -> Newline {
    return { .indentation = current_indentation };
}
