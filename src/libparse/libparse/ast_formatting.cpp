#include <libutl/common/utilities.hpp>
#include <libparse/ast.hpp>


DIRECTLY_DEFINE_FORMATTER_FOR(ast::expression::Match::Case) {
    return fmt::format_to(context.out(), "{} -> {}", value.pattern, value.handler);
}

DIRECTLY_DEFINE_FORMATTER_FOR(ast::expression::Lambda::Capture) {
    return std::visit(utl::Overload {
        [&](ast::expression::Lambda::Capture::By_pattern const& capture) {
            return fmt::format_to(context.out(), "{} = {}", capture.pattern, capture.expression);
        },
        [&](ast::expression::Lambda::Capture::By_reference const& capture) {
            return fmt::format_to(context.out(), "&{}", capture.variable);
        }
    }, value.value);
}

DIRECTLY_DEFINE_FORMATTER_FOR(ast::Function_argument) {
    return fmt::format_to(
        context.out(),
        "{}{}",
        value.name.transform("{} = "_format).value_or(""),
        value.expression);
}


DEFINE_FORMATTER_FOR(ast::Function_parameter) {
    return fmt::format_to(
        context.out(),
        "{}{}{}",
        value.pattern,
        value.type.transform(": {}"_format).value_or(""),
        value.default_argument.transform(" = {}"_format).value_or(""));
}

DEFINE_FORMATTER_FOR(ast::Mutability) {
    return utl::match(value.value,
        [&](ast::Mutability::Concrete const concrete) {
            return !concrete.is_mutable ? context.out() : fmt::format_to(context.out(), "mut ");
        },
        [&](ast::Mutability::Parameterized const parameterized) {
            return fmt::format_to(context.out(), "mut?{} ", parameterized.identifier);
        });
}

DEFINE_FORMATTER_FOR(ast::expression::Type_cast::Kind) {
    using enum ast::expression::Type_cast::Kind;
    switch (value) {
    case conversion:
        return fmt::format_to(context.out(), "as");
    case ascription:
        return fmt::format_to(context.out(), ":");
    default:
        utl::unreachable();
    }
}

DEFINE_FORMATTER_FOR(ast::Name) {
    return fmt::format_to(context.out(), "{}", value.identifier);
}


namespace {

    struct Expression_format_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(ast::expression::Literal<T> const& literal) {
            if constexpr (std::same_as<T, compiler::String>)
                return format("\"{}\"", literal.value);
            if constexpr (std::same_as<T, compiler::Character>)
                return format("'{}'", literal.value);
            else
                return format("{}", literal.value);
        }
        auto operator()(ast::expression::Array_literal const& literal) {
            return format("[{}]", literal.elements);
        }
        auto operator()(ast::expression::Self const&) {
            return format("self");
        }
        auto operator()(ast::expression::Variable const& variable) {
            return format("{}", variable.name);
        }
        auto operator()(ast::expression::Template_application const& application) {
            return format("{}[{}]", application.name, application.template_arguments);
        }
        auto operator()(ast::expression::Tuple const& tuple) {
            return format("({})", tuple.fields);
        }
        auto operator()(ast::expression::Invocation const& invocation) {
            return format("{}({})", invocation.invocable, invocation.arguments);
        }
        auto operator()(ast::expression::Struct_initializer const& initializer) {
            return format(
                "{} {{ {} }}",
                initializer.struct_type,
                initializer.member_initializers.span());
        }
        auto operator()(ast::expression::Binary_operator_invocation const& invocation) {
            return format("({} {} {})", invocation.left, invocation.op, invocation.right);
        }
        auto operator()(ast::expression::Struct_field_access const& access) {
           return format("{}.{}", access.base_expression, access.field_name);
        }
        auto operator()(ast::expression::Tuple_field_access const& access) {
            return format("{}.{}", access.base_expression, access.field_index);
        }
        auto operator()(ast::expression::Array_index_access const& access) {
            return format("{}.[{}]", access.base_expression, access.index_expression);
        }
        auto operator()(ast::expression::Method_invocation const& invocation) {
            format("{}.{}", invocation.base_expression, invocation.method_name);
            if (invocation.template_arguments.has_value())
                format("[{}]", *invocation.template_arguments);
            return format("({})", invocation.arguments);
        }
        auto operator()(ast::expression::Block const& block) {
            format("{{ ");
            for (auto const& side_effect : block.side_effect_expressions)
                format("{}; ", side_effect);
            return block.result_expression.has_value()
                ? format("{} }}", *block.result_expression)
                : format("}}");
        }
        auto operator()(ast::expression::Conditional const& conditional) {
            auto const& [condition, true_branch, false_branch] = conditional;
            format("if {} {}", condition, true_branch);
            if (false_branch.has_value())
                format(" else {}", *false_branch);
            return out;
        }
        auto operator()(ast::expression::Match const& match) {
            return format("match {} {{ {} }}", match.matched_expression, utl::formatting::delimited_range(match.cases, " "));
        }
        auto operator()(ast::expression::Type_cast const& cast) {
            return format("({} {} {})", cast.expression, cast.cast_kind, cast.target_type);
        }
        auto operator()(ast::expression::Let_binding const& let) {
            format("let {}", let.pattern);
            if (let.type.has_value())
                format(": {}", *let.type);
            return format(" = {}", let.initializer);
        }
        auto operator()(ast::expression::Conditional_let const& binding) {
            return format("let {} = {}", binding.pattern, binding.initializer);
        }
        auto operator()(ast::expression::Local_type_alias const& alias) {
            return format("alias {} = {}", alias.identifier, alias.aliased_type);
        }
        auto operator()(ast::expression::Lambda const& lambda) {
            return format(
                "\\{}{}{} -> {}",
                lambda.parameters,
                lambda.explicit_captures.empty() ? "" : " . ",
                lambda.explicit_captures,
                lambda.body);
        }
        auto operator()(ast::expression::Infinite_loop const& loop) {
            if (loop.label)
                format("{} ", loop.label->identifier);
            return format("loop {}", loop.body);
        }
        auto operator()(ast::expression::While_loop const& loop) {
            if (loop.label)
                format("{} ", loop.label->identifier);
            return format("while {} {}", loop.condition, loop.body);
        }
        auto operator()(ast::expression::For_loop const& loop) {
            if (loop.label)
                format("{} ", loop.label->identifier);
            return format("for {} in {} {}", loop.iterator, loop.iterable, loop.body);
        }
        auto operator()(ast::expression::Continue) {
            return format("continue");
        }
        auto operator()(ast::expression::Break const& break_) {
            format("break");
            if (break_.label)
                format(" {} loop", *break_.label);
            if (break_.result)
                format(" {}", *break_.result);
            return out;
        }
        auto operator()(ast::expression::Ret const& ret) {
            return format("ret {}", ret.returned_expression);
        }
        auto operator()(ast::expression::Discard const& discard) {
            return format("discard {}", discard.discarded_expression);
        }
        auto operator()(ast::expression::Sizeof const& sizeof_) {
            return format("sizeof({})", sizeof_.inspected_type);
        }
        auto operator()(ast::expression::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_expression);
        }
        auto operator()(ast::expression::Dereference const& dereference) {
            return format("*{}", dereference.dereferenced_expression);
        }
        auto operator()(ast::expression::Addressof const& addressof) {
            return format("addressof({})", addressof.lvalue);
        }
        auto operator()(ast::expression::Unsafe_dereference const& dereference) {
            return format("unsafe_dereference({})", dereference.pointer);
        }
        auto operator()(ast::expression::Placement_init const& init) {
            return format("{} <- {}", init.lvalue, init.initializer);
        }
        auto operator()(ast::expression::Move const& move) {
            return format("mov {}", move.lvalue);
        }
        auto operator()(ast::expression::Meta const& meta) {
            return format("meta({})", meta.expression);
        }
        auto operator()(ast::expression::Hole const&) {
            return format("???");
        }
    };


    struct Pattern_format_visitor : utl::formatting::Visitor_base {
        auto operator()(ast::pattern::Wildcard) {
            return format("_");
        }
        template <class T>
        auto operator()(ast::pattern::Literal<T> const& literal) {
            return std::invoke(Expression_format_visitor { { out } }, literal);
        }
        auto operator()(ast::pattern::Name const& name) {
            return format("{}{}", name.mutability, name.identifier);
        }
        auto operator()(ast::pattern::Constructor const& ctor) {
            return ctor.payload_pattern.has_value()
                ? format("{}({})", ctor.constructor_name, ctor.payload_pattern)
                : format("{}", ctor.constructor_name);
        }
        auto operator()(ast::pattern::Tuple const& tuple) {
            return format("({})", tuple.field_patterns);
        }
        auto operator()(ast::pattern::Slice const& slice) {
            return format("[{}]", slice.element_patterns);
        }
        auto operator()(ast::pattern::As const& as) {
            format("{} as ", as.aliased_pattern);
            return operator()(as.alias);
        }
        auto operator()(ast::pattern::Guarded const& guarded) {
            return format("{} if {}", guarded.guarded_pattern, guarded.guard);
        }
    };


    struct Type_format_visitor : utl::formatting::Visitor_base {
        auto operator()(ast::type::Floating)  { return format("Float");  }
        auto operator()(ast::type::Character) { return format("Char");   }
        auto operator()(ast::type::Boolean)   { return format("Bool");   }
        auto operator()(ast::type::String)    { return format("String"); }

        auto operator()(ast::type::Integer const integer) {
            switch (integer) {
            case ast::type::Integer::i8:  return format("I8");
            case ast::type::Integer::i16: return format("I16");
            case ast::type::Integer::i32: return format("I32");
            case ast::type::Integer::i64: return format("I64");
            case ast::type::Integer::u8:  return format("U8");
            case ast::type::Integer::u16: return format("U16");
            case ast::type::Integer::u32: return format("U32");
            case ast::type::Integer::u64: return format("U64");
            default:
                utl::unreachable();
            }
        }

        auto operator()(ast::type::Wildcard const&) {
            return format("_");
        }
        auto operator()(ast::type::Self const&) {
            return format("Self");
        }
        auto operator()(ast::type::Typename const& name) {
            return format("{}", name.name);
        }
        auto operator()(ast::type::Tuple const& tuple) {
            return format("({})", tuple.field_types);
        }
        auto operator()(ast::type::Array const& array) {
            return format("[{}; {}]", array.element_type, array.array_length);
        }
        auto operator()(ast::type::Slice const& slice) {
            return format("[{}]", slice.element_type);
        }
        auto operator()(ast::type::Function const& function) {
            return format("fn({}): {}", function.argument_types, function.return_type);
        }
        auto operator()(ast::type::Typeof const& typeof_) {
            return format("typeof({})", typeof_.inspected_expression);
        }
        auto operator()(ast::type::Instance_of const& instance_of) {
            return format("inst {}", utl::formatting::delimited_range(instance_of.classes, " + "));
        }
        auto operator()(ast::type::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_type);
        }
        auto operator()(ast::type::Pointer const& pointer) {
            return format("*{}{}", pointer.mutability, pointer.pointed_to_type);
        }
        auto operator()(ast::type::Template_application const& application) {
            return format("{}[{}]", application.name, application.arguments);
        }
    };

}


DEFINE_FORMATTER_FOR(ast::Expression) {
    return std::visit(Expression_format_visitor { { context.out() } }, value.value);
}
DEFINE_FORMATTER_FOR(ast::Pattern) {
    return std::visit(Pattern_format_visitor { { context.out() } }, value.value);
}
DEFINE_FORMATTER_FOR(ast::Type) {
    return std::visit(Type_format_visitor { { context.out() } }, value.value);
}


DEFINE_FORMATTER_FOR(ast::Module) {
    auto out = context.out();

    if (value.name)
        out = fmt::format_to(out, "module {}\n", *value.name);
    for (auto const& import : value.imports)
        out = fmt::format_to(out, "import \"{}\"\n", import.view());

    return fmt::format_to(out, "{}", utl::formatting::delimited_range(value.definitions, "\n\n"));
}
