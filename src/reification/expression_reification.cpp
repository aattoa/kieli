#include "utl/utilities.hpp"
#include "reification_internals.hpp"


namespace {

    struct Expression_reification_visitor {
        reification::Context & context;
        mir::Expression const& this_expression;

        auto operator()(mir::expression::Sizeof const& sizeof_) -> cir::Expression::Variant {
            cir::Type inspected_type = context.reify_type(sizeof_.inspected_type);
            return cir::expression::Literal<utl::Isize> {
                utl::safe_cast<utl::Isize>(inspected_type.size.get()) };
        }

        auto operator()(auto const&) -> cir::Expression::Variant {
            utl::abort();
        }
    };

}


auto reification::Context::reify_expression(mir::Expression const& expression) -> cir::Expression {
    return {
        .value       = std::visit(Expression_reification_visitor { *this, expression }, expression.value),
        .type        = reify_type(expression.type),
        .source_view = expression.source_view
    };
}
