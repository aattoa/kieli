#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>


namespace {
    struct Pattern_format_visitor {
        libformat::State& state;
        auto operator()(auto const&) -> void {
            utl::todo();
        }
    };
}


auto libformat::format_pattern(cst::Pattern const& pattern, libformat::State& state) -> void {
    utl::match(pattern.value, Pattern_format_visitor { state });
}
