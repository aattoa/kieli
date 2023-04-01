#include "utl/utilities.hpp"
#include "lir_formatting.hpp"


namespace {
    struct Expression_formatting_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(lir::expression::Constant<T> const& constant) {
            return format("constant {}", constant.value);
        }
        auto operator()(lir::expression::Tuple const& tuple) {
            return format("({})", tuple.elements);
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
            return format(" {} }}", block.result_expression);
        }
        auto operator()(lir::expression::Unconditional_jump const& jump) {
            return format("jump to offset {}", jump.target_offset);
        }
        auto operator()(lir::expression::Conditional_jump const& jump) {
            return format("if {} jump to offset {}", jump.condition, jump.target_offset);
        }
        auto operator()(lir::expression::Hole const&) {
            return format("???");
        }
    };
}


DEFINE_FORMATTER_FOR(lir::Expression) {
    return std::visit(Expression_formatting_visitor { { context.out() } }, value);
}