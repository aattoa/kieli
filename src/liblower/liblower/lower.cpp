#include <libutl/common/utilities.hpp>
#include <libreify/cir.hpp>
#include <liblower/lower.hpp>


namespace {
    [[nodiscard]]
    auto make_integer_constant(
        compiler::built_in_type::Integer const type,
        std::integral auto               const integer) -> lir::Expression
    {
        using enum compiler::built_in_type::Integer;
        switch (type) {
        case i8:  return utl::safe_cast<utl::I8> (integer);
        case i16: return utl::safe_cast<utl::I16>(integer);
        case i32: return utl::safe_cast<utl::I32>(integer);
        case i64: return utl::safe_cast<utl::I64>(integer);
        case u8:  return utl::safe_cast<utl::U8> (integer);
        case u16: return utl::safe_cast<utl::U16>(integer);
        case u32: return utl::safe_cast<utl::U32>(integer);
        case u64: return utl::safe_cast<utl::U64>(integer);
        default: utl::unreachable();
        }
    }

    [[nodiscard]]
    auto make_integer_range(compiler::built_in_type::Integer const type)
        -> utl::Pair<std::variant<utl::Isize, utl::Usize>>
    {
        static constexpr auto range = []<std::integral T>(utl::Type<T>) {
            using U = std::conditional_t<std::is_signed_v<T>, utl::Isize, utl::Usize>;
            return decltype(make_integer_range(type)) {
                utl::safe_cast<U>(std::numeric_limits<T>::min()),
                utl::safe_cast<U>(std::numeric_limits<T>::max()),
            };
        };
        using enum compiler::built_in_type::Integer;
        switch (type) {
        case i8:  return range(utl::type<utl::I8>);
        case i16: return range(utl::type<utl::I16>);
        case i32: return range(utl::type<utl::I32>);
        case i64: return range(utl::type<utl::I64>);
        case u8:  return range(utl::type<utl::U8>);
        case u16: return range(utl::type<utl::U16>);
        case u32: return range(utl::type<utl::U32>);
        case u64: return range(utl::type<utl::U64>);
        default: utl::unreachable();
        }
    }


    struct Expression_lowering_visitor {
        utl::diagnostics::Builder& diagnostics;
        lir::Node_arena          & node_arena;
        cir::Expression     const& this_expression;

        auto recurse(cir::Expression const& expression) -> lir::Expression {
            return std::visit(Expression_lowering_visitor { diagnostics, node_arena, expression }, expression.value);
        }
        auto recurse(utl::Wrapper<cir::Expression> const expression) -> utl::Wrapper<lir::Expression> {
            return node_arena.wrap(recurse(*expression));
        }
        [[nodiscard]]
        auto recurse() {
            return [this](auto const& expression) { return recurse(expression); };
        }

        auto operator()(compiler::Integer const& integer_literal) -> lir::Expression {
            auto const type = utl::get<compiler::built_in_type::Integer>(*this_expression.type.value);
            try {
                return make_integer_constant(type, integer_literal.value);
            }
            catch (utl::Safe_cast_invalid_argument const&) {
                auto const [min, max] = make_integer_range(type);
                diagnostics.emit_error({
                    .sections  = utl::to_vector({ utl::diagnostics::Text_section { .source_view = this_expression.source_view } }),
                    .message   = "The value of this integer literal is outside of the valid range for {}"_format(cir::to_string(this_expression.type)),
                    .help_note = "The valid range for {} is {}..{}"_format(cir::to_string(this_expression.type), min, max),
                });
            }
        }
        template <compiler::literal Literal>
        auto operator()(Literal const& literal) -> lir::Expression {
            return literal;
        }
        auto operator()(cir::expression::Tuple const& tuple) -> lir::Expression {
            return lir::expression::Tuple { .elements = utl::map(recurse(), tuple.fields) };
        }
        auto operator()(cir::expression::Loop const& loop) -> lir::Expression {
            return lir::expression::Loop { .body = recurse(loop.body) };
        }
        auto operator()(cir::expression::Break const& break_) -> lir::Expression {
            return lir::expression::Break { .result = recurse(break_.result) };
        }
        auto operator()(cir::expression::Continue const&) -> lir::Expression {
            return lir::expression::Continue {};
        }
        auto operator()(cir::expression::Let_binding const& let) -> lir::Expression {
            return recurse(*let.initializer);
        }
        auto operator()(cir::expression::Block const& block) -> lir::Expression {
            return lir::expression::Block {
                .side_effect_expressions    = utl::map(recurse(), block.side_effect_expressions),
                .result_expression          = recurse(block.result_expression),
                .result_object_frame_offset = block.result_object_frame_offset,
                .result_size                = block.result_expression->type.size.get(),
                .scope_size                 = block.scope_size.get(),
            };
        }
        auto operator()(cir::expression::Local_variable_reference const& local) -> lir::Expression {
            return lir::expression::Local_variable_bitcopy {
                .frame_offset = local.frame_offset,
                .byte_count   = this_expression.type.size.get(),
            };
        }
        auto operator()(cir::expression::Conditional const& conditional) -> lir::Expression {
            return lir::expression::Conditional {
                .condition    = recurse(conditional.condition),
                .true_branch  = recurse(conditional.true_branch),
                .false_branch = recurse(conditional.false_branch),
            };
        }
        auto operator()(cir::expression::Hole const&) -> lir::Expression {
            return lir::expression::Hole { .source_view = this_expression.source_view };
        }
    };

}


auto kieli::lower(Reify_result&& reify_result) -> Lower_result {
    auto node_arena = lir::Node_arena::with_default_page_size();
    std::vector<lir::Function> functions;

    for (cir::Function& function : reify_result.functions) {
        functions.push_back(lir::Function {
            .symbol = std::move(function.symbol),
            .body = std::visit(Expression_lowering_visitor {
                reify_result.compilation_info.get()->diagnostics,
                node_arena,
                function.body
            }, function.body.value)
        });
    }

    return Lower_result {
        .compilation_info = std::move(reify_result.compilation_info),
        .node_arena       = std::move(node_arena),
        .functions        = std::move(functions),
    };
}
