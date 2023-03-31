#include "utl/utilities.hpp"
#include "reification_internals.hpp"


namespace {

    struct Expression_reification_visitor {
        reification::Context & context;
        mir::Expression const& this_expression;

        auto recurse(mir::Expression const& expression) -> cir::Expression {
            return context.reify_expression(expression);
        }
        auto recurse(utl::Wrapper<mir::Expression> const expression) -> utl::Wrapper<cir::Expression> {
            return utl::wrap(recurse(*expression));
        }
        [[nodiscard]]
        auto recurse() noexcept {
            return [this](auto const& expression) { return recurse(expression); };
        }

        template <class T>
        auto operator()(mir::expression::Literal<T> const& literal) -> cir::Expression::Variant {
            return cir::expression::Literal<T> { literal.value };
        }

        auto operator()(mir::expression::Sizeof const& sizeof_) -> cir::Expression::Variant {
            cir::Type inspected_type = context.reify_type(sizeof_.inspected_type);
            return cir::expression::Literal<compiler::Unsigned_integer> { inspected_type.size.get() };
        }

        auto operator()(mir::expression::Block const& block) -> cir::Expression::Variant {
            auto const scope = context.scope();
            return cir::expression::Block {
                .side_effect_expressions = utl::map(recurse(), block.side_effect_expressions),
                .result_expression       = recurse(block.result_expression)
            };
        }

        auto operator()(mir::expression::Tuple const& tuple) -> cir::Expression::Variant {
            return cir::expression::Tuple { .fields = utl::map(recurse(), tuple.fields) };
        }

        auto operator()(mir::expression::Let_binding const& binding) -> cir::Expression::Variant {
            return cir::expression::Let_binding {
                .pattern     = context.reify_pattern(*binding.pattern),
                .initializer = recurse(binding.initializer)
            };
        }

        auto operator()(mir::expression::Local_variable_reference const& local) -> cir::Expression::Variant {
            if (auto const* const frame_offset = context.variable_frame_offsets.find(local.tag)) {
                return cir::expression::Local_variable_reference {
                    .frame_offset = frame_offset->get(),
                    .identifier   = local.identifier
                };
            }
            utl::unreachable();
        }

        auto operator()(mir::expression::Hole const&) -> cir::Expression::Variant {
            return cir::expression::Hole {};
        }

        auto operator()(auto const&) -> cir::Expression::Variant {
            utl::todo();
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
