#include "utl/utilities.hpp"
#include "representation/ast/ast.hpp"
#include "mir.hpp"

#include "phase/resolve/resolution_internals.hpp"


DIRECTLY_DEFINE_FORMATTER_FOR(mir::Function_parameter) {
    return fmt::format_to(context.out(), "{}: {}", value.pattern, value.type);
}

DIRECTLY_DEFINE_FORMATTER_FOR(mir::Struct::Member) {
    return fmt::format_to(context.out(), "{}{}: {}", value.is_public ? "pub " : "", value.name, value.type);
}

DIRECTLY_DEFINE_FORMATTER_FOR(mir::Enum_constructor) {
    return fmt::format_to(context.out(), "{}{}", value.name, value.payload_type.transform("({})"_format).value_or(""));
}

DIRECTLY_DEFINE_FORMATTER_FOR(mir::expression::Match::Case) {
    return fmt::format_to(context.out(), "{} -> {}", value.pattern, value.handler);
}


DEFINE_FORMATTER_FOR(mir::Class_reference) {
    return fmt::format_to(context.out(), "{}", value.info->name);
}

DEFINE_FORMATTER_FOR(mir::Mutability::Variant) {
    return utl::match(value,
        [&](mir::Mutability::Concrete const concrete) {
            return !concrete.is_mutable ? context.out() : fmt::format_to(context.out(), "mut ");
        },
        [&](mir::Mutability::Variable const variable) {
            return fmt::format_to(context.out(), "'mut{} ", variable.tag.value);
        },
        [&](mir::Mutability::Parameterized const parameterized) {
            return fmt::format_to(context.out(), "mut?'P{} {} ", parameterized.tag.value, parameterized.identifier);
        }
    );
}

DEFINE_FORMATTER_FOR(mir::Mutability) {
    return fmt::format_to(context.out(), "{}", *value.value);
}

DEFINE_FORMATTER_FOR(mir::Unification_variable_tag) {
    return fmt::format_to(context.out(), "'{}", value.value);
}

DEFINE_FORMATTER_FOR(mir::Template_argument) {
    auto out = context.out();
    if (value.name.has_value()) {
        out = fmt::format_to(out, "{} = ", *value.name);
    }
    return utl::match(
        value.value,

        [&](mir::Mutability const mutability) {
            return utl::match(
                *mutability.value,

                [&](mir::Mutability::Concrete const concrete) {
                    return fmt::format_to(out, "{}", concrete.is_mutable ? "mut" : "immut");
                },
                [&](mir::Mutability::Variable const variable) {
                    return fmt::format_to(context.out(), "'mut{}", variable.tag.value);
                },
                [&](mir::Mutability::Parameterized const parameterized) {
                    return fmt::format_to(context.out(), "mut?'P{} {}", parameterized.tag.value, parameterized.identifier);
                }
            );
        },
        [&](auto const& argument) {
            return fmt::format_to(out, "{}", argument);
        }
    );
}

DEFINE_FORMATTER_FOR(mir::Template_parameter) {
    auto out = context.out();

    utl::match(
        value.value,

        [&](mir::Template_parameter::Type_parameter const& parameter) {
            out = fmt::format_to(context.out(), "'P{} {}", value.reference_tag.value, value.name);
            out = parameter.classes.empty() ? out : fmt::format_to(out, ": {}", utl::formatting::delimited_range(parameter.classes, " + "));
        },
        [&](mir::Template_parameter::Value_parameter const& parameter) {
            out = fmt::format_to(out, "{}: {}", value.name, parameter.type);
        },
        [&](mir::Template_parameter::Mutability_parameter const&){
            out = fmt::format_to(out, "{}: mut", value.name);
        }
    );

    return value.default_argument.has_value() ? fmt::format_to(out, " = {}", *value.default_argument) : out;
}


namespace {

    struct Expression_format_visitor : utl::formatting::Visitor_base {
        template <class T>
        auto operator()(mir::expression::Literal<T> const& literal) {
            return format("{}", literal.value);
        }
        auto operator()(mir::expression::Literal<char> const& literal) {
            return format("'{}'", literal.value);
        }
        auto operator()(mir::expression::Literal<compiler::String> const& literal) {
            return format("\"{}\"", literal.value);
        }
        auto operator()(mir::expression::Function_reference const& function) {
            if (function.info->template_instantiation_info.has_value()) {
                return format("{}[{}]", function.info->name, function.info->template_instantiation_info->template_arguments);
            }
            else {
                return format("{}", function.info->name);
            }
        }
        auto operator()(mir::expression::Tuple const& tuple) {
            return format("({})", tuple.fields);
        }
        auto operator()(mir::expression::Block const& block) {
            format("{{ ");
            for (mir::Expression const& side_effect : block.side_effects) {
                format("{}; ", side_effect);
            }
            return format("{}}}", block.result.transform("{} "_format).value_or(""));
        }
        auto operator()(mir::expression::Let_binding const& let) {
            return format("let {}: {} = {}", let.pattern, let.type, let.initializer);
        }
        auto operator()(mir::expression::Conditional const& conditional) {
            return format("if {} {} else {}", conditional.condition, conditional.true_branch, conditional.false_branch);
        }
        auto operator()(mir::expression::Match const& match) {
            return format("match {} {{ {} }}", match.matched_expression, match.cases);
        }
        auto operator()(mir::expression::Array_literal const& array) {
            return format("[{}]", array.elements);
        }
        auto operator()(mir::expression::Local_variable_reference const& variable) {
            return format("{}", variable.identifier);
        }
        auto operator()(mir::expression::Struct_initializer const& initializer) {
            return format("{} {{ {} }}", initializer.struct_type, initializer.initializers);
        }
        auto operator()(mir::expression::Struct_member_access const& access) {
            return format("{}.{}", access.base_expression, access.member_identifier);
        }
        auto operator()(mir::expression::Tuple_member_access const& access) {
            return format("{}.{}", access.base_expression, access.member_index);
        }
        auto operator()(mir::expression::Direct_invocation const& invocation) {
            operator()(invocation.function);
            return format("({})", invocation.arguments);
        }
        auto operator()(mir::expression::Indirect_invocation const& invocation) {
            return format("{}({})", invocation.invocable, invocation.arguments);
        }
        auto operator()(mir::expression::Enum_constructor_reference const& reference) {
            return format("{}", reference.constructor.name);
        }
        auto operator()(mir::expression::Direct_enum_constructor_invocation const& invocation) {
            return format("{}({})", invocation.constructor.name, invocation.arguments);
        }
        auto operator()(mir::expression::Sizeof const& sizeof_) {
            return format("sizeof({})", sizeof_.inspected_type);
        }
        auto operator()(mir::expression::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_expression);
        }
        auto operator()(mir::expression::Dereference const& dereference) {
            return format("*{}", dereference.dereferenced_expression);
        }
        auto operator()(mir::expression::Addressof const& addressof) {
            return format("addressof({})", addressof.lvalue);
        }
        auto operator()(mir::expression::Unsafe_dereference const& dereference) {
            return format("unsafe_dereference({})", dereference.pointer);
        }
        auto operator()(mir::expression::Move const& move) {
            return format("mov {}", move.lvalue);
        }
        auto operator()(mir::expression::Hole const&) {
            return format("???");
        }
    };

    struct Pattern_format_visitor : utl::formatting::Visitor_base {
        auto operator()(mir::pattern::Wildcard const&) {
            return format("_");
        }
        template <class T>
        auto operator()(mir::pattern::Literal<T> const& literal) {
            return format("{}", literal.value);
        }
        auto operator()(mir::pattern::Literal<compiler::String> const& literal) {
            return format("\"{}\"", literal.value);
        }
        auto operator()(mir::pattern::Literal<char> const& literal) {
            return format("'{}'", literal.value);
        }
        auto operator()(mir::pattern::Name const& name) {
            return format("{}{}", name.mutability, name.identifier);
        }
        auto operator()(mir::pattern::Tuple const& tuple) {
            return format("({})", tuple.field_patterns);
        }
        auto operator()(mir::pattern::Slice const& slice) {
            return format("[{}]", slice.element_patterns);
        }
        auto operator()(mir::pattern::As const& as) {
            format("{} as", as.aliased_pattern);
            return operator()(as.alias);
        }
        auto operator()(mir::pattern::Guarded const& guarded) {
            return format("{} if {}", guarded.guarded_pattern, guarded.guard);
        }
        auto operator()(mir::pattern::Enum_constructor const& ctor) {
            return format(
                "{}{}",
                ctor.constructor.name,
                ctor.payload_pattern.transform("({})"_format).value_or("")
            );
        }
    };

    struct Type_format_visitor : utl::formatting::Visitor_base {
        auto operator()(mir::type::Integer const integer) {
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
            default:
                utl::unreachable();
            }
        }
        auto operator()(mir::type::Floating)         { return format("Float"); }
        auto operator()(mir::type::Character)        { return format("Char"); }
        auto operator()(mir::type::Boolean)          { return format("Bool"); }
        auto operator()(mir::type::String)           { return format("String"); }
        auto operator()(mir::type::Self_placeholder) { return format("Self"); }
        auto operator()(mir::type::Array const& array) {
            return format("[{}; {}]", array.element_type, array.array_length);
        }
        auto operator()(mir::type::Slice const& slice) {
            return format("[{}]", slice.element_type);
        }
        auto operator()(mir::type::Reference const& reference) {
            return format("&{}{}", reference.mutability, reference.referenced_type);
        }
        auto operator()(mir::type::Pointer const& pointer) {
            return format("*{}{}", pointer.mutability, pointer.pointed_to_type);
        }
        auto operator()(mir::type::Function const& function) {
            return format("fn({}): {}", function.parameter_types, function.return_type);
        }
        auto operator()(mir::type::Tuple const& tuple) {
            return format("({})", tuple.field_types);
        }
        auto operator()(utl::one_of<mir::type::Structure, mir::type::Enumeration> auto const& type) {
            if (type.info->template_instantiation_info.has_value()) {
                return format("{}[{}]", type.info->name, type.info->template_instantiation_info->template_arguments);
            }
            else {
                return format("{}", type.info->name);
            }
        }
        auto operator()(mir::type::General_unification_variable const& variable) {
            return format("'T{}", variable.tag.value);
        }
        auto operator()(mir::type::Integral_unification_variable const& variable) {
            return format("'I{}", variable.tag.value);
        }
        auto operator()(mir::type::Template_parameter_reference const& reference) {
            return format("'P{} {}", reference.tag.value, reference.identifier);
        }
    };

}


DEFINE_FORMATTER_FOR(mir::Expression) {
    auto out = fmt::format_to(context.out(), "(");
    out = std::visit(Expression_format_visitor { { out } }, value.value);
    return fmt::format_to(out, "): {}", value.type);
}

DEFINE_FORMATTER_FOR(mir::Pattern) {
    return std::visit(Pattern_format_visitor { { context.out() } }, value.value);
}

DEFINE_FORMATTER_FOR(mir::Type::Variant) {
    return std::visit(Type_format_visitor { { context.out() } }, value);
}

DEFINE_FORMATTER_FOR(mir::Type) {
    return fmt::format_to(context.out(), "{}", *value.value);
}


DEFINE_FORMATTER_FOR(mir::Function) {
    return fmt::format_to(
        context.out(),
        "fn {}({}): {} = {}",
        value.name,
        value.signature.parameters,
        value.signature.return_type,
        value.body
    );
}

DEFINE_FORMATTER_FOR(mir::Struct) {
    return fmt::format_to(
        context.out(),
        "struct {} = {}",
        value.name,
        value.members
    );
}

DEFINE_FORMATTER_FOR(mir::Enum) {
    return fmt::format_to(
        context.out(),
        "enum {} = {}",
        value.name,
        utl::formatting::delimited_range(value.constructors, " | ")
    );
}

DEFINE_FORMATTER_FOR(mir::Alias) {
    return fmt::format_to(
        context.out(),
        "alias {} = {}",
        value.name,
        value.aliased_type
    );
}

DEFINE_FORMATTER_FOR(mir::Typeclass) {
    return fmt::format_to(
        context.out(),
        "class {} {{ /*TODO*/ }}",
        value.name
    );
}

DEFINE_FORMATTER_FOR(mir::Implementation) {
    return fmt::format_to(
        context.out(),
        "impl {} {{ /*TODO*/ }}",
        value.self_type
    );
}

DEFINE_FORMATTER_FOR(mir::Instantiation) {
    return fmt::format_to(
        context.out(),
        "inst {} for {} {{ /*TODO*/ }}",
        value.class_reference,
        value.self_type
    );
}