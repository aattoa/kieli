#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>


namespace {
    struct Type_format_visitor {
        libformat::State& state;
        auto operator()(auto const&) -> void {
            utl::todo();
        }
    };
}


auto libformat::format_type(cst::Type const& type, libformat::State& state) -> void {
    utl::match(type.value, Type_format_visitor { state });
}
