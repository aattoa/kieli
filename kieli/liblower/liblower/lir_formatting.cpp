#include <libutl/common/utilities.hpp>
#include <liblower/lir.hpp>


namespace {
    struct Expression_formatting_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(lir::expression::Constant<T> const& constant) {
            return format("constant {}", constant.value);
        }
        auto operator()(lir::expression::Tuple const& tuple) {
            return format("({})", tuple.elements);
        }
        auto operator()(lir::expression::Loop const& loop) {
            return format("loop {}", loop.body);
        }
        auto operator()(lir::expression::Break const& break_) {
            return format("break {}", break_.result);
        }
        auto operator()(lir::expression::Continue const&) {
            return format("continue");
        }
        auto operator()(lir::expression::Direct_invocation const& invocation) {
            return format("{}({})", invocation.function_symbol, invocation.arguments);
        }
        auto operator()(lir::expression::Indirect_invocation const& invocation) {
            return format("({})({})", invocation.invocable, invocation.arguments);
        }
        auto operator()(lir::expression::Local_variable_bitcopy const& local) {
            return format("copy offset: {} bytes: {}", local.frame_offset, local.byte_count);
        }
        auto operator()(lir::expression::Block const& block) {
            format("{{ ");
            for (auto const& side_effect : block.side_effect_expressions)
                format("{}; ", side_effect);
            return format(" {} (res {}, pop {}) }}", block.result_expression, block.result_size, block.scope_size);
        }
        auto operator()(lir::expression::Conditional const& conditional) {
            return format("if {} {} else {}", conditional.condition, conditional.true_branch, conditional.false_branch);
        }
        auto operator()(lir::expression::Hole const&) {
            return format("???");
        }
    };
}


DEFINE_FORMATTER_FOR(lir::Expression) {
    return std::visit(Expression_formatting_visitor { { context.out() } }, value);
}
