#include <libutl/common/utilities.hpp>
#include <libresolve/hir.hpp>
#include <libresolve/resolution_internals.hpp> // FIXME?

#define DECLARE_FORMATTER(...)                                                           \
    template <>                                                                          \
    struct std::formatter<__VA_ARGS__> : utl::formatting::Formatter_base {               \
        auto format(__VA_ARGS__ const&, auto& context) const -> decltype(context.out()); \
    }

#define DEFINE_FORMATTER(...)                                                         \
    auto hir::format_to(__VA_ARGS__ const& value, std::string& string)->void          \
    {                                                                                 \
        std::format_to(std::back_inserter(string), "{}", value);                      \
    }                                                                                 \
    auto std::formatter<__VA_ARGS__>::format(__VA_ARGS__ const& value, auto& context) \
        const->decltype(context.out())

DECLARE_FORMATTER(hir::Expression);
DECLARE_FORMATTER(hir::Type);
DECLARE_FORMATTER(hir::Pattern);
DECLARE_FORMATTER(hir::Mutability);
DECLARE_FORMATTER(hir::Function);
DECLARE_FORMATTER(hir::Struct);
DECLARE_FORMATTER(hir::Enum);
DECLARE_FORMATTER(hir::Alias);
DECLARE_FORMATTER(hir::Typeclass);
DECLARE_FORMATTER(hir::Implementation);
DECLARE_FORMATTER(hir::Instantiation);
DECLARE_FORMATTER(hir::Unification_variable_tag);
DECLARE_FORMATTER(hir::Template_parameter);
DECLARE_FORMATTER(hir::Template_argument);
DECLARE_FORMATTER(hir::Function_parameter);

template <>
struct std::formatter<hir::expression::Match::Case> : utl::formatting::Formatter_base {
    auto format(hir::expression::Match::Case const& match_case, auto& context) const
    {
        return std::format_to(context.out(), "{} -> {};", match_case.pattern, match_case.handler);
    }
};

template <>
struct std::formatter<hir::Class_reference> : utl::formatting::Formatter_base {
    auto format(hir::Class_reference const& reference, auto& context) const
    {
        return std::format_to(context.out(), "{}", reference.info->name);
    }
};

template <>
struct std::formatter<hir::Unification_type_variable_state::Unsolved>
    : utl::formatting::Formatter_base {
    auto format(hir::Unification_type_variable_state::Unsolved const& unsolved, auto& context) const
    {
        std::format_to(
            context.out(),
            "'{}{}",
            std::invoke([kind = unsolved.kind] {
                switch (kind) {
                case hir::Unification_type_variable_kind::general:
                    return 'T';
                case hir::Unification_type_variable_kind::integral:
                    return 'I';
                default:
                    utl::unreachable();
                }
            }),
            unsolved.tag.value);
        if (!unsolved.classes.empty()) {
            std::format_to(
                context.out(), ": {}", utl::formatting::delimited_range(unsolved.classes, " + "));
        }
        return context.out();
    }
};

template <>
struct std::formatter<hir::Enum_constructor> : utl::formatting::Formatter_base {
    auto format(hir::Enum_constructor const& constructor, auto& context) const
    {
        std::format_to(context.out(), "{}::{}", constructor.enum_type, constructor.name);
        return constructor.payload_type.has_value()
                 ? std::format_to(context.out(), "({})", *constructor.payload_type)
                 : context.out();
    }
};

template <>
struct std::formatter<hir::Struct::Member> : utl::formatting::Formatter_base {
    auto format(hir::Struct::Member const& member, auto& context) const
    {
        return std::format_to(
            context.out(), "{}{}: {}", member.is_public ? "pub " : "", member.name, member.type);
    }
};

namespace {
    template <class Out>
    struct Expression_format_visitor {
        Out out;

        explicit Expression_format_visitor(Out&& out) noexcept : out { std::move(out) } {}

        template <compiler::literal Literal>
        auto operator()(Literal const& literal)
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(hir::expression::Function_reference const& function)
        {
            std::format_to(out, "{}", function.info->name);
            if (function.info->template_instantiation_info.has_value()) {
                std::format_to(
                    out, "[{}]", function.info->template_instantiation_info->template_arguments);
            }
        }

        auto operator()(hir::expression::Tuple const& tuple)
        {
            std::format_to(out, "({})", tuple.fields);
        }

        auto operator()(hir::expression::Loop const& loop)
        {
            std::format_to(out, "loop {}", loop.body);
        }

        auto operator()(hir::expression::Break const& break_)
        {
            std::format_to(out, "break {}", break_.result);
        }

        auto operator()(hir::expression::Continue const&)
        {
            std::format_to(out, "continue");
        }

        auto operator()(hir::expression::Block const& block)
        {
            std::format_to(out, "{{ ");
            for (hir::Expression const& side_effect : block.side_effect_expressions) {
                std::format_to(out, "{}; ", side_effect);
            }
            std::format_to(out, "{} }}", block.result_expression);
        }

        auto operator()(hir::expression::Let_binding const& let)
        {
            std::format_to(out, "let {}: {} = {}", let.pattern, let.type, let.initializer);
        }

        auto operator()(hir::expression::Conditional const& conditional)
        {
            std::format_to(
                out,
                "if {} {} else {}",
                conditional.condition,
                conditional.true_branch,
                conditional.false_branch);
        }

        auto operator()(hir::expression::Match const& match)
        {
            std::format_to(
                out,
                "match {} {{ {} }}",
                match.matched_expression,
                utl::formatting::delimited_range(match.cases, " "));
        }

        auto operator()(hir::expression::Array_literal const& array)
        {
            std::format_to(out, "[{}]", array.elements);
        }

        auto operator()(hir::expression::Local_variable_reference const& variable)
        {
            std::format_to(out, "{}", variable.identifier);
        }

        auto operator()(hir::expression::Struct_initializer const& initializer)
        {
            std::format_to(out, "{} {{ {} }}", initializer.struct_type, initializer.initializers);
        }

        auto operator()(hir::expression::Struct_field_access const& access)
        {
            std::format_to(out, "{}.{}", access.base_expression, access.field_name);
        }

        auto operator()(hir::expression::Tuple_field_access const& access)
        {
            std::format_to(out, "{}.{}", access.base_expression, access.field_index);
        }

        auto operator()(hir::expression::Direct_invocation const& invocation)
        {
            operator()(invocation.function);
            std::format_to(out, "({})", invocation.arguments);
        }

        auto operator()(hir::expression::Indirect_invocation const& invocation)
        {
            std::format_to(out, "{}({})", invocation.invocable, invocation.arguments);
        }

        auto operator()(hir::expression::Enum_constructor_reference const& reference)
        {
            std::format_to(
                out, "{}::{}", reference.constructor.enum_type, reference.constructor.name);
        }

        auto operator()(hir::expression::Direct_enum_constructor_invocation const& invocation)
        {
            std::format_to(out, "{}({})", invocation.constructor.name, invocation.arguments);
        }

        auto operator()(hir::expression::Sizeof const& sizeof_)
        {
            std::format_to(out, "sizeof({})", sizeof_.inspected_type);
        }

        auto operator()(hir::expression::Reference const& reference)
        {
            std::format_to(out, "&{} {}", reference.mutability, reference.referenced_expression);
        }

        auto operator()(hir::expression::Dereference const& dereference)
        {
            std::format_to(out, "*{}", dereference.dereferenced_expression);
        }

        auto operator()(hir::expression::Addressof const& addressof)
        {
            std::format_to(out, "addressof({})", addressof.lvalue);
        }

        auto operator()(hir::expression::Unsafe_dereference const& dereference)
        {
            std::format_to(out, "dereference({})", dereference.pointer);
        }

        auto operator()(hir::expression::Move const& move)
        {
            std::format_to(out, "mov {}", move.lvalue);
        }

        auto operator()(hir::expression::Hole const&)
        {
            std::format_to(out, R"(???)");
        }
    };

    template <class Out>
    struct Type_format_visitor {
        Out out;

        explicit Type_format_visitor(Out&& out) noexcept : out { std::move(out) } {}

        auto operator()(compiler::built_in_type::Integer const integer)
        {
            std::format_to(out, "{}", compiler::built_in_type::integer_string(integer));
        }

        auto operator()(compiler::built_in_type::Floating const&)
        {
            std::format_to(out, "Float");
        }

        auto operator()(compiler::built_in_type::Character const&)
        {
            std::format_to(out, "Char");
        }

        auto operator()(compiler::built_in_type::Boolean const&)
        {
            std::format_to(out, "Bool");
        }

        auto operator()(compiler::built_in_type::String const&)
        {
            std::format_to(out, "String");
        }

        auto operator()(hir::type::Self_placeholder const&)
        {
            std::format_to(out, "Self");
        }

        auto operator()(hir::type::Array const& array)
        {
            std::format_to(out, "[{}; {}]", array.element_type, array.array_length);
        }

        auto operator()(hir::type::Slice const& slice)
        {
            std::format_to(out, "[{}]", slice.element_type);
        }

        auto operator()(hir::type::Reference const& reference)
        {
            std::format_to(out, "&{} {}", reference.mutability, reference.referenced_type);
        }

        auto operator()(hir::type::Pointer const& pointer)
        {
            std::format_to(out, "*{} {}", pointer.mutability, pointer.pointed_to_type);
        }

        auto operator()(hir::type::Function const& function)
        {
            std::format_to(out, "fn({}): {}", function.parameter_types, function.return_type);
        }

        auto operator()(hir::type::Tuple const& tuple)
        {
            std::format_to(out, "({})", tuple.field_types);
        }

        auto operator()(utl::one_of<hir::type::Structure, hir::type::Enumeration> auto const& type)
        {
            std::format_to(out, "{}", type.info->name);
            if (type.info->template_instantiation_info.has_value()) {
                std::format_to(
                    out, "[{}]", type.info->template_instantiation_info->template_arguments);
            }
        }

        auto operator()(hir::type::Unification_variable const& variable)
        {
            std::format_to(out, "{}", variable.state->as_unsolved());
        }

        auto operator()(hir::type::Template_parameter_reference const& reference)
        {
            std::format_to(
                out,
                "'P{} {}",
                reference.tag.value,
                reference.identifier.get().has_value() ? reference.identifier.get()->view()
                                                       : "implicit");
        }
    };

    template <class Out>
    struct Pattern_format_visitor {
        Out out;

        explicit Pattern_format_visitor(Out&& out) noexcept : out { std::move(out) } {}

        template <compiler::literal Literal>
        auto operator()(Literal const& literal)
        {
            std::format_to(out, "{}", literal.value);
        }

        auto operator()(hir::pattern::Wildcard const&)
        {
            std::format_to(out, "_");
        }

        auto operator()(hir::pattern::Name const& name)
        {
            std::format_to(out, "{} {}", name.mutability, name.identifier);
        }

        auto operator()(hir::pattern::Tuple const& tuple)
        {
            std::format_to(out, "({})", tuple.field_patterns);
        }

        auto operator()(hir::pattern::Slice const& slice)
        {
            std::format_to(out, "[{}]", slice.element_patterns);
        }

        auto operator()(hir::pattern::As const& as)
        {
            std::format_to(out, "{} as ", as.aliased_pattern);
            operator()(as.alias);
        }

        auto operator()(hir::pattern::Guarded const& guarded)
        {
            std::format_to(out, "{} if {}", guarded.guarded_pattern, guarded.guard);
        }

        auto operator()(hir::pattern::Enum_constructor const& ctor)
        {
            std::format_to(out, "{}::{}", ctor.constructor.enum_type, ctor.constructor.name);
            if (ctor.payload_pattern.has_value()) {
                std::format_to(out, "({})", *ctor.payload_pattern);
            }
        }
    };
} // namespace

DEFINE_FORMATTER(hir::Expression)
{
    std::format_to(context.out(), "(");
    utl::match(value.value, Expression_format_visitor { context.out() });
    return std::format_to(context.out(), "): {}", value.type);
}

DEFINE_FORMATTER(hir::Type)
{
    utl::match(*value.flattened_value(), Type_format_visitor { context.out() });
    return context.out();
}

DEFINE_FORMATTER(hir::Pattern)
{
    utl::match(value.value, Pattern_format_visitor { context.out() });
    return context.out();
}

DEFINE_FORMATTER(hir::Mutability)
{
    utl::match(
        *value.flattened_value(),
        [&](hir::Mutability::Concrete const& concrete) {
            std::format_to(context.out(), "{}", concrete.is_mutable ? "mut" : "immut");
        },
        [&](hir::Mutability::Parameterized const& parameterized) {
            std::format_to(
                context.out(), "mut?{}'{}", parameterized.tag.value, parameterized.identifier);
        },
        [&](hir::Mutability::Variable const& variable) {
            std::format_to(context.out(), "'{}mut", variable.state->as_unsolved().tag.value);
        });
    return context.out();
}

DEFINE_FORMATTER(hir::Function)
{
    return std::format_to(
        context.out(),
        "fn {}{}({}): {} = {}",
        value.signature.name,
        value.signature.is_template() ? "[{}]"_format(value.signature.template_parameters) : "",
        value.signature.parameters,
        value.signature.return_type,
        value.body);
}

DEFINE_FORMATTER(hir::Struct)
{
    return std::format_to(context.out(), "struct {} = {}", value.name, value.members);
}

DEFINE_FORMATTER(hir::Enum)
{
    return std::format_to(
        context.out(),
        "enum {} = {}",
        value.name,
        utl::formatting::delimited_range(value.constructors, " | "));
}

DEFINE_FORMATTER(hir::Alias)
{
    return std::format_to(context.out(), "alias {} = {}", value.name, value.aliased_type);
}

DEFINE_FORMATTER(hir::Typeclass)
{
    if (value.type_signatures.empty() && value.function_signatures.empty()) {
        return std::format_to(context.out(), "class {} {{}}", value.name);
    }
    std::format_to(context.out(), "class {} {{\n", value.name);
    for (auto const& [name, signature] : value.type_signatures) {
        std::format_to(context.out(), "{}", name);
        if (!signature.classes.empty()) {
            std::format_to(context.out(), ": {}\n", signature.classes);
        }
    }
    for (auto const& [name, signature] : value.function_signatures) {
        std::format_to(context.out(), "fn {}", name);
        if (!signature.template_parameters.empty()) {
            std::format_to(context.out(), "[{}]", signature.template_parameters);
        }
        std::format_to(context.out(), "({}): {}", signature.parameters, signature.return_type);
    }
    return std::format_to(context.out(), "}}");
}

DEFINE_FORMATTER(hir::Implementation)
{
    return std::format_to(context.out(), "impl {} {{ /*TODO*/ }}", value.self_type);
}

DEFINE_FORMATTER(hir::Instantiation)
{
    return std::format_to(
        context.out(), "inst {} for {} {{ /*TODO*/ }}", value.class_reference, value.self_type);
}

DEFINE_FORMATTER(hir::Unification_variable_tag)
{
    return std::format_to(context.out(), "'{}", value.value);
}

DEFINE_FORMATTER(hir::Template_parameter)
{
    utl::match(
        value.value,
        [&](hir::Template_parameter::Type_parameter const& parameter) {
            if (parameter.name.has_value()) {
                std::format_to(context.out(), "{}", *parameter.name);
            }
            else {
                std::format_to(context.out(), "implicit");
            }

            if (!parameter.classes.empty()) {
                std::format_to(
                    context.out(),
                    ": {}",
                    utl::formatting::delimited_range(parameter.classes, " + "));
            }
        },
        [&](hir::Template_parameter::Mutability_parameter const& parameter) {
            std::format_to(context.out(), "{}: mut", parameter.name);
        },
        [&](hir::Template_parameter::Value_parameter const& parameter) {
            std::format_to(context.out(), "{}: {}", parameter.name, parameter.type);
        });
    return std::format_to(context.out(), " '{}", value.reference_tag.value);
}

DEFINE_FORMATTER(hir::Template_argument)
{
    return utl::match(value.value, [&](auto const& argument) {
        return std::format_to(context.out(), "{}", argument);
    });
}

DEFINE_FORMATTER(hir::Function_parameter)
{
    return std::format_to(context.out(), "{}: {}", value.pattern, value.type);
}
