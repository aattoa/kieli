#include <libutl/common/utilities.hpp>
#include <libreify/reification_internals.hpp>


namespace {

    struct Expression_reification_visitor {
        reification::Context & context;
        mir::Expression const& this_expression;

        auto recurse(mir::Expression const& expression) -> cir::Expression {
            return context.reify_expression(expression);
        }
        auto recurse(utl::Wrapper<mir::Expression> const expression) -> utl::Wrapper<cir::Expression> {
            return context.wrap(recurse(*expression));
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
            cir::Type const inspected_type = context.reify_type(sizeof_.inspected_type);
            return cir::expression::Literal<compiler::Unsigned_integer> { inspected_type.size.get() };
        }

        auto operator()(mir::expression::Block const& block) -> cir::Expression::Variant {
            auto const result_object_frame_offset = context.current_frame_offset;

            // Reserve space for the result object
            context.current_frame_offset += context.reify_type(block.result_expression->type).size.get();

            auto const old_frame_offset = context.current_frame_offset;

            auto side_effects = utl::map(recurse(), block.side_effect_expressions);
            auto result       = recurse(block.result_expression);
            auto scope_size   = context.current_frame_offset - old_frame_offset;

            context.current_frame_offset = old_frame_offset;

            return cir::expression::Block {
                .side_effect_expressions    = std::move(side_effects),
                .result_expression          = std::move(result),
                .scope_size                 = cir::Type::Size { scope_size },
                .result_object_frame_offset = result_object_frame_offset.get()
            };
        }

        auto operator()(mir::expression::Tuple const& tuple) -> cir::Expression::Variant {
            return cir::expression::Tuple { .fields = utl::map(recurse(), tuple.fields) };
        }
        auto operator()(mir::expression::Loop const& loop) -> cir::Expression::Variant {
            return cir::expression::Loop { .body = recurse(loop.body) };
        }
        auto operator()(mir::expression::Break const& break_) -> cir::Expression::Variant {
            return cir::expression::Break { .result = recurse(break_.result) };
        }
        auto operator()(mir::expression::Continue const&) -> cir::Expression::Variant {
            return cir::expression::Continue {};
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

        auto operator()(mir::expression::Conditional const& conditional) -> cir::Expression::Variant {
            return cir::expression::Conditional {
                .condition    = recurse(conditional.condition),
                .true_branch  = recurse(conditional.true_branch),
                .false_branch = recurse(conditional.false_branch),
            };
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