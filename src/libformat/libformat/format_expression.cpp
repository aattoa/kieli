#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>


namespace {
    struct Expression_format_visitor {
        libformat::State& state;
        auto operator()(auto const&) -> void {
            utl::todo();
        }
    };
}


auto libformat::format_expression(cst::Expression const& expression, State& state) -> void {
    utl::match(expression.value, Expression_format_visitor { state });
}
