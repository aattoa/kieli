#include "utl/utilities.hpp"
#include "hir.hpp"


DIRECTLY_DEFINE_FORMATTER_FOR(hir::Function_argument) {
    return value.name
        ? std::format_to(context.out(), "{} = {}", *value.name, value.expression)
        : std::format_to(context.out(), "{}", value.expression);
}


DEFINE_FORMATTER_FOR(hir::Function_parameter) {
    return std::format_to(
        context.out(),
        "{}: {}{}",
        value.pattern,
        value.type,
        value.default_value.transform(" = {}"_format).value_or("")
    );
}

DEFINE_FORMATTER_FOR(hir::Implicit_template_parameter::Tag) {
    return std::format_to(context.out(), "X#{}", value.value);
}

DEFINE_FORMATTER_FOR(hir::Implicit_template_parameter) {
    return std::vformat_to(
        context.out(),
        value.classes.empty() ? "{}" : "{}: {}",
        std::make_format_args(
            value.tag,
            utl::fmt::delimited_range(value.classes, " + ")
        )
    );
}


namespace {

    struct Expression_format_visitor : utl::fmt::Visitor_base {
        template <class T>
        auto operator()(hir::expression::Literal<T> const& literal) {
            return format("{}", literal.value);
        }
        auto operator()(hir::expression::Literal<char> const& literal) {
            return format("'{}'", literal.value);
        }
        auto operator()(hir::expression::Literal<compiler::String> const& literal) {
            return format("\"{}\"", literal.value);
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
            if (break_.label.has_value()) {
                format(" {} loop", *break_.label);
            }
            if (break_.result.has_value()) {
                format(" {}", *break_.result);
            }
            return out;
        }
        auto operator()(hir::expression::Continue const&) {
            return format("continue");
        }
        auto operator()(hir::expression::Block const& block) {
            format("{{ ");
            for (auto const& side_effect : block.side_effects) {
                format("{}; ", side_effect);
            }
            return format("{}}}", block.result.transform("{} "_format).value_or(""));
        }
        auto operator()(hir::expression::Invocation const& invocation) {
            return format("{}({})", invocation.invocable, invocation.arguments);
        }
        auto operator()(hir::expression::Struct_initializer const& initializer) {
            return format(
                "{} {{ {} }}",
                initializer.struct_type,
                initializer.member_initializers.span()
            );
        }
        auto operator()(hir::expression::Binary_operator_invocation const& invocation) {
            return format("({} {} {})", invocation.left, invocation.op, invocation.right);
        }
        auto operator()(hir::expression::Member_access_chain const& chain) {
            using Chain = hir::expression::Member_access_chain;

            format("({}", chain.base_expression);
            for (Chain::Accessor const& accessor : chain.accessors) {
                utl::match(
                    accessor.value,

                    [this](Chain::Tuple_field const& field) {
                        format(".{}", field.index);
                    },
                    [this](Chain::Struct_field const& field) {
                        format(".{}", field.identifier);
                    },
                    [this](Chain::Array_index const& index) {
                        format(".[{}]", index.expression);
                    }
                );
            }
            return format(")");
        }
        auto operator()(hir::expression::Method_invocation const& invocation) {
            format("{}.{}", invocation.base_expression, invocation.method_name);
            if (invocation.template_arguments.has_value()) {
                format("[{}]", *invocation.template_arguments);
            }
            return format("({})", invocation.arguments);
        }
        auto operator()(hir::expression::Conditional const& conditional) {
            return format("if {} {} else {}", conditional.condition, conditional.true_branch, conditional.false_branch);
        }
        auto operator()(hir::expression::Match const& match) {
            format("match {} {{ ", match.matched_expression);
            for (auto& match_case : match.cases) {
                format("{} -> {}", match_case.pattern, match_case.handler);
            }
            return format(" }}");
        }
        auto operator()(hir::expression::Template_application const& application) {
            return format("{}[{}]", application.name, application.template_arguments);
        }
        auto operator()(hir::expression::Type_cast const& cast) {
            return format("({} {} {})", cast.expression, cast.cast_kind, cast.target_type);
        }
        auto operator()(hir::expression::Let_binding const& let) {
            return format(
                "let {}{} = {}",
                let.pattern,
                let.type.transform(": {}"_format).value_or(""),
                let.initializer
            );
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
        auto operator()(hir::expression::Unsafe_dereference const& dereference) {
            return format("unsafe_dereference({})", dereference.pointer);
        }
        auto operator()(hir::expression::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_expression);
        }
        auto operator()(hir::expression::Dereference const& dereference) {
            return format("(*{})", dereference.dereferenced_expression);
        }
        auto operator()(hir::expression::Placement_init const& init) {
            return format("{} <- {}", init.lvalue, init.initializer);
        }
        auto operator()(hir::expression::Meta const& meta) {
            return format("meta {}", meta.expression);
        }
        auto operator()(hir::expression::Hole const&) {
            return format("???");
        }
    };


    struct Type_format_visitor : utl::fmt::Visitor_base {
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
        auto operator()(hir::type::Implicit_parameter_reference const& parameter) {
            return format("{}", parameter.tag);
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
        auto operator()(hir::type::Typeof const& typeof) {
            return format("type_of({})", typeof.inspected_expression);
        }
        auto operator()(hir::type::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_type);
        }
        auto operator()(hir::type::Pointer const& pointer) {
            return format("*{}{}", pointer.mutability, pointer.pointed_to_type);
        }
        auto operator()(hir::type::Instance_of const& instance_of) {
            return format("inst {}", utl::fmt::delimited_range(instance_of.classes, " + "));
        }
        auto operator()(hir::type::Template_application const& application) {
            return format("{}[{}]", application.name, application.arguments);
        }
    };


    struct Pattern_format_visitor : utl::fmt::Visitor_base {
        template <class T>
        auto operator()(hir::pattern::Literal<T> const& literal) {
            return std::invoke(Expression_format_visitor { { out } }, hir::expression::Literal { literal.value });
        }
        auto operator()(hir::pattern::Wildcard const&) {
            return format("_");
        }
        auto operator()(hir::pattern::Name const& name) {
            return format("{}{}", name.mutability, name.identifier);
        }
        auto operator()(hir::pattern::Constructor const& ctor) {
            return ctor.payload_pattern.has_value()
                ? format("ctor {}({})", ctor.constructor_name, ctor.payload_pattern)
                : format("ctor {}", ctor.constructor_name);
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