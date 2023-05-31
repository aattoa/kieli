#include <libutl/common/utilities.hpp>
#include <libdesugar/hir.hpp>


DIRECTLY_DEFINE_FORMATTER_FOR(hir::Function_argument) {
    auto out = context.out();
    if (value.name.has_value())
        out = fmt::format_to(out, "{} = ", *value.name);
    return fmt::format_to(out, "{}", value.expression);
}

DEFINE_FORMATTER_FOR(hir::Function_parameter) {
    return fmt::format_to(
        context.out(),
        "{}: {}{}",
        value.pattern,
        value.type,
        value.default_argument.transform(" = {}"_format).value_or(""));
}


namespace {
    struct Expression_format_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(hir::expression::Literal<T> const& literal) {
            if constexpr (std::same_as<T, compiler::String>)
                return format("\"{}\"", literal.value);
            if constexpr (std::same_as<T, kieli::Character>)
                return format("'{}'", literal.value);
            else
                return format("{}", literal.value);
        }
        auto operator()(hir::expression::Array_literal const& literal) {
            return format("[{}]", literal.elements);
        }
        auto operator()(hir::expression::Self const&) {
            return format("self");
        }
        auto operator()(hir::expression::Variable const& variable) {
            return format("{}", variable.name);
        }
        auto operator()(hir::expression::Tuple const& tuple) {
            return format("({})", tuple.fields);
        }
        auto operator()(hir::expression::Loop const& loop) {
            return format("loop {{ {} }}", loop.body);
        }
        auto operator()(hir::expression::Break const& break_) {
            format("break");
            if (break_.label.has_value())
                format(" {} loop", *break_.label);
            return format(" {}", break_.result);
        }
        auto operator()(hir::expression::Continue const&) {
            return format("continue");
        }
        auto operator()(hir::expression::Block const& block) {
            format("{{ ");
            for (auto const& side_effect : block.side_effect_expressions)
                format("{}; ", side_effect);
            return format("{} }}", block.result_expression);
        }
        auto operator()(hir::expression::Invocation const& invocation) {
            return format("{}({})", invocation.invocable, invocation.arguments);
        }
        auto operator()(hir::expression::Struct_initializer const& initializer) {
            return format(
                "{} {{ {} }}",
                initializer.struct_type,
                initializer.member_initializers.span());
        }
        auto operator()(hir::expression::Binary_operator_invocation const& invocation) {
            return format("({} {} {})", invocation.left, invocation.op, invocation.right);
        }
        auto operator()(hir::expression::Struct_field_access const& access) {
            return format("{}.{}", access.base_expression, access.field_name);
        }
        auto operator()(hir::expression::Tuple_field_access const& access) {
            return format("{}.{}", access.base_expression, access.field_index);
        }
        auto operator()(hir::expression::Array_index_access const& access) {
            return format("{}.[{}]", access.base_expression, access.index_expression);
        }
        auto operator()(hir::expression::Method_invocation const& invocation) {
            format("{}.{}", invocation.base_expression, invocation.method_name);
            if (invocation.template_arguments.has_value())
                format("[{}]", *invocation.template_arguments);
            return format("({})", invocation.arguments);
        }
        auto operator()(hir::expression::Conditional const& conditional) {
            return format("if {} {} else {}", conditional.condition, conditional.true_branch, conditional.false_branch);
        }
        auto operator()(hir::expression::Match const& match) {
            format("match {} {{ ", match.matched_expression);
            for (auto const& match_case : match.cases)
                format("{} -> {}", match_case.pattern, match_case.handler);
            return format(" }}");
        }
        auto operator()(hir::expression::Template_application const& application) {
            return format("{}[{}]", application.name, application.template_arguments);
        }
        auto operator()(hir::expression::Type_cast const& cast) {
            return format("({} {} {})", cast.expression, cast.cast_kind, cast.target_type);
        }
        auto operator()(hir::expression::Let_binding const& let) {
            format("let {}", let.pattern);
            if (let.type.has_value())
                format(": {}", *let.type);
            return format(" = {}", let.initializer);
        }
        auto operator()(hir::expression::Local_type_alias const& alias) {
            return format("alias {} = {}", alias.identifier, alias.aliased_type);
        }
        auto operator()(hir::expression::Ret const& ret) {
            return format("ret {}", ret.returned_expression);
        }
        auto operator()(hir::expression::Sizeof const& sizeof_) {
            return format("sizeof({})", sizeof_.inspected_type);
        }
        auto operator()(hir::expression::Addressof const& addressof) {
            return format("addressof({})", addressof.lvalue);
        }
        auto operator()(hir::expression::Reference_dereference const& dereference) {
            return format("(*{})", dereference.dereferenced_expression);
        }
        auto operator()(hir::expression::Pointer_dereference const& dereference) {
            return format("dereference({})", dereference.pointer);
        }
        auto operator()(hir::expression::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_expression);
        }
        auto operator()(hir::expression::Placement_init const& init) {
            return format("{} <- {}", init.lvalue, init.initializer);
        }
        auto operator()(hir::expression::Unsafe const& unsafe) {
            return format("unsafe {}", unsafe.expression);
        }
        auto operator()(hir::expression::Move const& move) {
            return format("mov {}", move.lvalue);
        }
        auto operator()(hir::expression::Meta const& meta) {
            return format("meta {}", meta.expression);
        }
        auto operator()(hir::expression::Hole const&) {
            return format("???");
        }
    };

    struct Type_format_visitor : utl::formatting::Visitor_base {
        auto operator()(hir::type::Floating)  { return format("Float");  }
        auto operator()(hir::type::Character) { return format("Char");   }
        auto operator()(hir::type::Boolean)   { return format("Bool");   }
        auto operator()(hir::type::String)    { return format("String"); }
        auto operator()(hir::type::Wildcard)  { return format("_");      }
        auto operator()(hir::type::Self)      { return format("Self");   }
        auto operator()(hir::type::Integer const integer) {
            switch (integer) {
            case hir::type::Integer::i8:  return format("I8");
            case hir::type::Integer::i16: return format("I16");
            case hir::type::Integer::i32: return format("I32");
            case hir::type::Integer::i64: return format("I64");
            case hir::type::Integer::u8:  return format("U8");
            case hir::type::Integer::u16: return format("U16");
            case hir::type::Integer::u32: return format("U32");
            case hir::type::Integer::u64: return format("U64");
            default:
                utl::unreachable();
            }
        }
        auto operator()(hir::type::Typename const& type) {
            return format("{}", type.name);
        }
        auto operator()(hir::type::Tuple const& tuple) {
            return format("({})", tuple.field_types);
        }
        auto operator()(hir::type::Array const& array) {
            return format("[{}; {}]", array.element_type, array.array_length);
        }
        auto operator()(hir::type::Slice const& slice) {
            return format("[{}]", slice.element_type);
        }
        auto operator()(hir::type::Function const& function) {
            return format("fn({}): {}", function.argument_types, function.return_type);
        }
        auto operator()(hir::type::Typeof const& typeof_) {
            return format("type_of({})", typeof_.inspected_expression);
        }
        auto operator()(hir::type::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_type);
        }
        auto operator()(hir::type::Pointer const& pointer) {
            return format("*{}{}", pointer.mutability, pointer.pointed_to_type);
        }
        auto operator()(hir::type::Instance_of const& instance_of) {
            return format("inst {}", utl::formatting::delimited_range(instance_of.classes, " + "));
        }
        auto operator()(hir::type::Template_application const& application) {
            return format("{}[{}]", application.name, application.arguments);
        }
    };

    struct Pattern_format_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(hir::pattern::Literal<T> const& literal) {
            return std::invoke(Expression_format_visitor { { out } }, hir::expression::Literal<T> { literal.value });
        }
        auto operator()(hir::pattern::Wildcard const&) {
            return format("_");
        }
        auto operator()(hir::pattern::Name const& name) {
            return format("{}{}", name.mutability, name.identifier);
        }
        auto operator()(hir::pattern::Constructor const& ctor) {
            return ctor.payload_pattern.has_value()
                   ? format("{}({})", ctor.constructor_name, ctor.payload_pattern)
                   : format("{}", ctor.constructor_name);
        }
        auto operator()(hir::pattern::Abbreviated_constructor const& ctor) {
            return ctor.payload_pattern.has_value()
                   ? format("::{}({})", ctor.constructor_name, ctor.payload_pattern)
                   : format("::{}", ctor.constructor_name);
        }
        auto operator()(hir::pattern::Tuple const& tuple) {
            return format("({})", tuple.field_patterns);
        }
        auto operator()(hir::pattern::Slice const& slice) {
            return format("[{}]", slice.element_patterns);
        }
        auto operator()(hir::pattern::As const& as) {
            format("{} as ", as.aliased_pattern);
            return operator()(as.alias);
        }
        auto operator()(hir::pattern::Guarded const& guarded) {
            return format("{} if {}", guarded.guarded_pattern, guarded.guard);
        }
    };
}


DEFINE_FORMATTER_FOR(hir::Expression) {
    return std::visit(Expression_format_visitor { { context.out() } }, value.value);
}
DEFINE_FORMATTER_FOR(hir::Type) {
    return std::visit(Type_format_visitor { { context.out() } }, value.value);
}
DEFINE_FORMATTER_FOR(hir::Pattern) {
    return std::visit(Pattern_format_visitor { { context.out() } }, value.value);
}
