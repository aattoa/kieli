#include "utl/utilities.hpp"
#include "cir_formatting.hpp"


namespace {
    struct Expression_formatting_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(cir::expression::Literal<T> const& literal) {
            if constexpr (std::same_as<T, compiler::String>)
                return format("\"{}\"", literal.value);
            if constexpr (std::same_as<T, compiler::Character>)
                return format("'{}'", literal.value);
            else
                return format("{}", literal.value);
        }
        auto operator()(cir::expression::Block const& block) {
            format("{{");
            for (auto const& side_effect : block.side_effect_expressions)
                format(" {};", side_effect);
            return format("{} }}", block.result_expression);
        }
        auto operator()(cir::expression::Tuple const& tuple) {
            return format("({})", tuple.fields);
        }
        auto operator()(cir::expression::Local_variable_reference const& local) {
            return format("{} offset {}", local.identifier, local.frame_offset);
        }
        auto operator()(cir::expression::Let_binding const& let) {
            return format("let {}: {} = {}", let.pattern, let.initializer->type, let.initializer);
        }
        auto operator()(cir::expression::Hole const&) {
            return format("???");
        }
    };
    struct Pattern_formatting_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(cir::pattern::Literal<T> const& literal) {
            return format("{}", literal.value);
        }
        auto operator()(cir::pattern::Tuple const& tuple) {
            return format("({})", tuple.field_patterns);
        }
        auto operator()(cir::pattern::Exhaustive const&) {
            return format("_");
        }
    };
    struct Type_formatting_visitor : utl::formatting::Visitor_base {
        auto operator()(cir::type::Integer const integer) {
            using enum mir::type::Integer;
            switch (integer) {
                case i8:  return format("I8");
                case i16: return format("I16");
                case i32: return format("I32");
                case i64: return format("I64");
                case u8:  return format("U8");
                case u16: return format("U16");
                case u32: return format("U32");
                case u64: return format("U64");
                default: utl::unreachable();
            }
        }
        auto operator()(cir::type::Floating)  { return format("Float"); }
        auto operator()(cir::type::Character) { return format("Char"); }
        auto operator()(cir::type::Boolean)   { return format("Bool"); }
        auto operator()(cir::type::String)    { return format("String"); }
        auto operator()(cir::type::Tuple const& tuple) {
            return format("({})", tuple.field_types);
        }
        auto operator()(cir::type::Pointer const& pointer) {
            return format("*{}", pointer.pointed_to_type);
        }
        auto operator()(cir::type::Enum_reference const&) -> decltype(out) {
            utl::todo();
        }
        auto operator()(cir::type::Struct_reference const&) -> decltype(out) {
            utl::todo();
        }
    };
}


DEFINE_FORMATTER_FOR(cir::Expression) {
    return std::visit(Expression_formatting_visitor { { context.out() } }, value.value);
}
DEFINE_FORMATTER_FOR(cir::Pattern) {
    return std::visit(Pattern_formatting_visitor { { context.out() } }, value.value);
}
DEFINE_FORMATTER_FOR(cir::Type) {
    return std::visit(Type_formatting_visitor { { context.out() } }, *value.value);
}