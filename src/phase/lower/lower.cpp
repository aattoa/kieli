#include "utl/utilities.hpp"
#include "representation/cir/cir.hpp"
#include "representation/cir/cir_formatting.hpp"
#include "lower.hpp"


namespace {
    [[nodiscard]]
    auto make_integer_constant(
        cir::type::Integer const type,
        std::integral auto const integer) -> lir::Expression
    {
        using enum cir::type::Integer;
        using lir::expression::Constant;
        switch (type) {
        case i8:  return Constant<utl::I8>  { utl::safe_cast<utl::I8> (integer) };
        case i16: return Constant<utl::I16> { utl::safe_cast<utl::I16>(integer) };
        case i32: return Constant<utl::I32> { utl::safe_cast<utl::I32>(integer) };
        case i64: return Constant<utl::I64> { utl::safe_cast<utl::I64>(integer) };
        case u8:  return Constant<utl::U8>  { utl::safe_cast<utl::U8> (integer) };
        case u16: return Constant<utl::U16> { utl::safe_cast<utl::U16>(integer) };
        case u32: return Constant<utl::U32> { utl::safe_cast<utl::U32>(integer) };
        case u64: return Constant<utl::U64> { utl::safe_cast<utl::U64>(integer) };
        default: utl::unreachable();
        }
    }

    [[nodiscard]]
    auto make_integer_range(cir::type::Integer const type)
        -> utl::Pair<std::variant<utl::Isize, utl::Usize>>
    {
        static constexpr auto range = []<std::integral T>(utl::Type<T>) {
            using U = std::conditional_t<std::is_signed_v<T>, utl::Isize, utl::Usize>;
            return decltype(make_integer_range(type)) {
                utl::safe_cast<U>(std::numeric_limits<T>::min()),
                utl::safe_cast<U>(std::numeric_limits<T>::max()),
            };
        };
        using enum cir::type::Integer;
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


    auto lower_expression(
        cir::Expression           const&,
        utl::diagnostics::Builder      &,
        utl::Source               const&) -> lir::Expression;

    struct Expression_lowering_visitor {
        utl::diagnostics::Builder& diagnostics;
        utl::Source         const& source;
        cir::Expression     const& this_expression;

        auto recurse(cir::Expression const& expression) const -> lir::Expression {
            return lower_expression(expression, diagnostics, source);
        }
        auto recurse(utl::Wrapper<cir::Expression> const expression) const -> utl::Wrapper<lir::Expression> {
            return utl::wrap(recurse(*expression));
        }
        [[nodiscard]]
        auto recurse() const {
            return [*this](auto const& expression) { return recurse(expression); };
        }


        template <utl::one_of<compiler::Signed_integer, compiler::Unsigned_integer, compiler::Integer_of_unknown_sign> T>
        auto operator()(cir::expression::Literal<T> const& integer_literal) -> lir::Expression {
            auto const type = utl::get<cir::type::Integer>(*this_expression.type.value);
            try {
                return make_integer_constant(type, integer_literal.value.value);
            }
            catch (utl::Safe_cast_invalid_argument const&) {
                auto const [min, max] = make_integer_range(type);
                diagnostics.emit_simple_error({
                    .erroneous_view      = this_expression.source_view,
                    .source              = source,
                    .message             = "The value of this integer literal is outside of the valid range for {}",
                    .message_arguments   = fmt::make_format_args(this_expression.type),
                    .help_note           = "The valid range for {} is {}..{}",
                    .help_note_arguments = fmt::make_format_args(this_expression.type, min, max),
                });
            }
        }
        template <class T>
        auto operator()(cir::expression::Literal<T> const& literal) -> lir::Expression {
            return lir::expression::Constant<T> { literal.value };
        }
        auto operator()(cir::expression::Tuple const& tuple) -> lir::Expression {
            return lir::expression::Tuple { .elements = utl::map(recurse(), tuple.fields) };
        }
        auto operator()(cir::expression::Let_binding const& let) -> lir::Expression {
            return recurse(*let.initializer);
        }
        auto operator()(cir::expression::Block const& block) -> lir::Expression {
            return lir::expression::Block {
                .side_effect_expressions = utl::map(recurse(), block.side_effect_expressions),
                .result_expression       = recurse(block.result_expression)
            };
        }
        auto operator()(cir::expression::Local_variable_reference const& local) -> lir::Expression {
            return lir::expression::Local_variable_bitcopy {
                .frame_offset = local.frame_offset,
                .byte_count   = this_expression.type.size.get()
            };
        }
        auto operator()(cir::expression::Hole const&) -> lir::Expression {
            return lir::expression::Hole { .source_view = this_expression.source_view };
        }
    };

    auto lower_expression(
        cir::Expression           const& expression,
        utl::diagnostics::Builder      & diagnostics,
        utl::Source               const& source) -> lir::Expression
    {
        return std::visit(
            Expression_lowering_visitor { diagnostics, source, expression },
            expression.value);
    }
}


auto compiler::lower(Reify_result&& reify_result) -> Lower_result {
    Lower_result lower_result {
        .diagnostics  = std::move(reify_result.diagnostics),
        .source       = std::move(reify_result.source),
        .node_context = lir::Node_context {}
    };

    for (cir::Function& function : reify_result.functions) {
        lower_result.functions.push_back(lir::Function {
            .symbol = std::move(function.symbol),
            .body   = lower_expression(function.body, lower_result.diagnostics, lower_result.source),
        });
    }

    return lower_result;
}