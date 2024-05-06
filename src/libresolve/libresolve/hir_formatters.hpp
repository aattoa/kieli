#pragma once

#include <libutl/utilities.hpp>
#include <libresolve/hir.hpp>
#include <libresolve/resolution_internals.hpp>

#define LIBRESOLVE_DECLARE_FORMATTER(...)                                                \
    template <>                                                                          \
    struct std::formatter<__VA_ARGS__> : utl::fmt::Formatter_base {                      \
        auto format(__VA_ARGS__ const&, auto& context) const -> decltype(context.out()); \
    }

#define LIBRESOLVE_DEFINE_FORMATTER(...)                                              \
    auto std::formatter<__VA_ARGS__>::format(__VA_ARGS__ const& value, auto& context) \
        const -> decltype(context.out())

LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Pattern);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Expression);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Type);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Type::Variant);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Mutability);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Mutability::Variant);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Function_parameter);
LIBRESOLVE_DECLARE_FORMATTER(libresolve::hir::Function_argument);

namespace libresolve::dtl {
    template <class Out>
    struct Expression_format_visitor {
        Out out;

        auto operator()(kieli::literal auto const& literal)
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(hir::expression::Array_literal const& literal)
        {
            std::format_to(out, "[{}]", literal.elements);
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
            std::format_to(out, "{{");
            for (hir::Expression const& side_effect : block.side_effects) {
                std::format_to(out, " {};", side_effect);
            }
            std::format_to(out, " {} }}", block.result);
        }

        auto operator()(hir::expression::Let_binding const& let)
        {
            std::format_to(out, "let {}: {} = {}", let.pattern, let.type, let.initializer);
        }

        auto operator()(hir::expression::Match const& match)
        {
            std::format_to(out, "match {} {{", match.expression);
            for (auto const& match_case : match.cases) {
                std::format_to(out, " {} -> {}", match_case.pattern, match_case.expression);
            }
            std::format_to(out, " }}");
        }

        auto operator()(hir::expression::Variable_reference const& variable)
        {
            std::format_to(out, "{}", variable.identifier);
        }

        auto operator()(hir::expression::Function_reference const& reference)
        {
            std::format_to(out, "{}", reference.info->name);
        }

        auto operator()(hir::expression::Indirect_invocation const& invocation)
        {
            std::format_to(out, "{}({:n})", invocation.function, invocation.arguments);
        }

        auto operator()(hir::expression::Direct_invocation const& invocation)
        {
            std::format_to(out, "{}({:n})", invocation.function_info->name, invocation.arguments);
        }

        auto operator()(hir::expression::Sizeof const& sizeof_)
        {
            std::format_to(out, "sizeof({})", sizeof_.inspected_type);
        }

        auto operator()(hir::expression::Addressof const& addressof)
        {
            std::format_to(out, "(&{} {})", addressof.mutability, addressof.place_expression);
        }

        auto operator()(hir::expression::Dereference const& dereference)
        {
            std::format_to(out, "(*{})", dereference.reference_expression);
        }

        auto operator()(hir::expression::Hole const&)
        {
            std::format_to(out, R"(???)");
        }

        auto operator()(hir::expression::Error const&)
        {
            std::format_to(out, "ERROR-EXPRESSION");
        }
    };

    template <class Out>
    struct Pattern_format_visitor {
        Out out;

        auto operator()(kieli::literal auto const& literal)
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(hir::pattern::Wildcard const&)
        {
            std::format_to(out, "_");
        }

        auto operator()(hir::pattern::Tuple const& tuple)
        {
            std::format_to(out, "({:n})", tuple.field_patterns);
        }

        auto operator()(hir::pattern::Slice const& slice)
        {
            std::format_to(out, "[{:n}]", slice.patterns);
        }

        auto operator()(hir::pattern::Name const& name)
        {
            std::format_to(out, "{} {}", name.mutability, name.identifier);
        }

        auto operator()(hir::pattern::Alias const& alias)
        {
            std::format_to(out, "{} as {} {}", alias.pattern, alias.mutability, alias.identifier);
        }

        auto operator()(hir::pattern::Guarded const& guarded)
        {
            std::format_to(out, "{} if {}", guarded.guarded_pattern, guarded.guard_expression);
        }
    };

    template <class Out>
    struct Type_format_visitor {
        Out out;

        auto operator()(kieli::built_in_type::Integer const integer)
        {
            std::format_to(out, "{}", kieli::built_in_type::integer_name(integer));
        }

        auto operator()(kieli::built_in_type::Floating const&)
        {
            std::format_to(out, "Float");
        }

        auto operator()(kieli::built_in_type::Character const&)
        {
            std::format_to(out, "Char");
        }

        auto operator()(kieli::built_in_type::Boolean const&)
        {
            std::format_to(out, "Bool");
        }

        auto operator()(kieli::built_in_type::String const&)
        {
            std::format_to(out, "String");
        }

        auto operator()(hir::type::Array const& array)
        {
            std::format_to(out, "[{}; {}]", array.element_type, array.length);
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
            std::format_to(out, "*{} {}", pointer.mutability, pointer.pointee_type);
        }

        auto operator()(hir::type::Function const& function)
        {
            std::format_to(out, "fn({:n}): {}", function.parameter_types, function.return_type);
        }

        auto operator()(hir::type::Enumeration const& enumeration)
        {
            std::format_to(out, "{}", enumeration.info->name);
        }

        auto operator()(hir::type::Tuple const& tuple)
        {
            std::format_to(out, "({:n})", tuple.types);
        }

        auto operator()(hir::type::Parameterized const& parameterized)
        {
            std::format_to(out, "template-parameter-{}", parameterized.tag.get());
        }

        auto operator()(hir::type::Variable const& variable)
        {
            std::format_to(out, "?{}", variable.tag.get());
        }

        auto operator()(hir::type::Error const&)
        {
            std::format_to(out, "ERROR-TYPE");
        }
    };
} // namespace libresolve::dtl

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Expression)
{
    std::format_to(context.out(), "(");
    std::visit(libresolve::dtl::Expression_format_visitor { context.out() }, value.variant);
    std::format_to(context.out(), ": ");
    std::visit(libresolve::dtl::Type_format_visitor { context.out() }, *value.type.variant);
    return std::format_to(context.out(), ")");
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Pattern)
{
    std::visit(libresolve::dtl::Pattern_format_visitor { context.out() }, value.variant);
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Type::Variant)
{
    std::visit(libresolve::dtl::Type_format_visitor { context.out() }, value);
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Mutability::Variant)
{
    return std::visit(
        utl::Overload {
            [&](libresolve::hir::mutability::Concrete const concrete) {
                return std::format_to(context.out(), "{}", concrete);
            },
            [&](libresolve::hir::mutability::Parameterized const& parameterized) {
                return std::format_to(context.out(), "mut?{}", parameterized.tag.get());
            },
            [&](libresolve::hir::mutability::Error const&) {
                return std::format_to(context.out(), "mut?ERROR");
            },
            [&](libresolve::hir::mutability::Variable const& variable) {
                return std::format_to(context.out(), "?mut{}", variable.tag.get());
            },
        },
        value);
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Type)
{
    return std::format_to(context.out(), "{}", *value.variant);
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Mutability)
{
    return std::format_to(context.out(), "{}", *value.variant);
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Function_parameter)
{
    std::format_to(context.out(), "{}: {}", value.pattern, value.type);
    if (value.default_argument.has_value()) {
        std::format_to(context.out(), " = {}", value.default_argument.value());
    }
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(libresolve::hir::Function_argument)
{
    if (value.name.has_value()) {
        std::format_to(context.out(), "{} = ", value.name.value());
    }
    return std::format_to(context.out(), "{}", value.expression);
}

#undef LIBRESOLVE_DECLARE_FORMATTER
#undef LIBRESOLVE_DEFINE_FORMATTER
