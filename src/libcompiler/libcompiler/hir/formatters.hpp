#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/hir/hir.hpp>

namespace kieli::hir::dtl {
    template <class T>
    struct With_arena {
        std::reference_wrapper<Arena const> arena;
        std::reference_wrapper<T const>     object;

        [[nodiscard]] auto get() const noexcept -> T const&
        {
            return object.get();
        }

        [[nodiscard]] auto operator->() const noexcept -> T const*
        {
            return std::addressof(get());
        }

        template <class Other>
        [[nodiscard]] auto wrap(Other const& other) const noexcept -> With_arena<Other>
        {
            return { arena, std::cref(other) };
        }
    };
} // namespace kieli::hir::dtl

#define LIBRESOLVE_DECLARE_FORMATTER(...)                                           \
    template <>                                                                     \
    struct std::formatter<kieli::hir::dtl::With_arena<__VA_ARGS__>> {               \
        static constexpr auto parse(auto& context)                                  \
        {                                                                           \
            return context.begin();                                                 \
        }                                                                           \
        static auto format(kieli::hir::dtl::With_arena<__VA_ARGS__>, auto& context) \
            -> decltype(context.out());                                             \
    }

#define LIBRESOLVE_DEFINE_FORMATTER(...)                                   \
    auto std::formatter<kieli::hir::dtl::With_arena<__VA_ARGS__>>::format( \
        kieli::hir::dtl::With_arena<__VA_ARGS__> value, auto& context) -> decltype(context.out())

LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Pattern_variant);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Pattern_id);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Pattern);

LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Expression_variant);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Expression_id);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Expression);

LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Type_variant);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Type_id);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Type);

LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Mutability_variant);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Mutability_id);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Mutability);

LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Function_parameter);
LIBRESOLVE_DECLARE_FORMATTER(kieli::hir::Function_argument);

template <class T>
    requires std::formattable<kieli::hir::dtl::With_arena<T>, char>
struct std::formatter<kieli::hir::dtl::With_arena<std::vector<T>>> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(kieli::hir::dtl::With_arena<std::vector<T>> const value, auto& context)
        -> decltype(context.out())
    {
        auto const wrap = [&](auto const& x) { return value.wrap(x); };
        return std::format_to(
            context.out(), "{:n}", std::views::transform(value.object.get(), wrap));
    }
};

namespace kieli::hir::dtl {
    template <class Out>
    struct Expression_format_visitor {
        Out          out;
        Arena const& arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(arena), std::cref(x) };
        }

        auto operator()(kieli::literal auto const& literal) const
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(hir::expression::Array_literal const& literal) const
        {
            std::format_to(out, "[{}]", wrap(literal.elements));
        }

        auto operator()(hir::expression::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.fields));
        }

        auto operator()(hir::expression::Loop const& loop) const
        {
            std::format_to(out, "loop {}", wrap(loop.body));
        }

        auto operator()(hir::expression::Break const& break_) const
        {
            std::format_to(out, "break {}", wrap(break_.result));
        }

        auto operator()(hir::expression::Continue const&) const
        {
            std::format_to(out, "continue");
        }

        auto operator()(hir::expression::Block const& block) const
        {
            std::format_to(out, "{{");
            for (hir::Expression const& side_effect : block.side_effects) {
                std::format_to(out, " {};", wrap(side_effect));
            }
            std::format_to(out, " {} }}", wrap(block.result));
        }

        auto operator()(hir::expression::Let_binding const& let) const
        {
            std::format_to(
                out, "let {}: {} = {}", wrap(let.pattern), wrap(let.type), wrap(let.initializer));
        }

        auto operator()(hir::expression::Match const& match) const
        {
            std::format_to(out, "match {} {{", wrap(match.expression));
            for (auto const& match_case : match.cases) {
                std::format_to(
                    out, " {} -> {}", wrap(match_case.pattern), wrap(match_case.expression));
            }
            std::format_to(out, " }}");
        }

        auto operator()(hir::expression::Variable_reference const& variable) const
        {
            std::format_to(out, "{}", variable.name);
        }

        auto operator()(hir::expression::Function_reference const& reference) const
        {
            std::format_to(out, "{}", reference.name);
        }

        auto operator()(hir::expression::Indirect_invocation const& invocation) const
        {
            std::format_to(out, "{}({})", wrap(invocation.function), wrap(invocation.arguments));
        }

        auto operator()(hir::expression::Direct_invocation const& invocation) const
        {
            std::format_to(out, "{}({})", invocation.function_name, wrap(invocation.arguments));
        }

        auto operator()(hir::expression::Sizeof const& sizeof_) const
        {
            std::format_to(out, "sizeof({})", wrap(sizeof_.inspected_type));
        }

        auto operator()(hir::expression::Addressof const& addressof) const
        {
            std::format_to(
                out, "(&{} {})", wrap(addressof.mutability), wrap(addressof.place_expression));
        }

        auto operator()(hir::expression::Dereference const& dereference) const
        {
            std::format_to(out, "(*{})", wrap(dereference.reference_expression));
        }

        auto operator()(hir::expression::Defer const& defer) const
        {
            std::format_to(out, "defer {}", wrap(defer.expression));
        }

        auto operator()(hir::expression::Hole const&) const
        {
            std::format_to(out, R"(???)");
        }

        auto operator()(hir::expression::Error const&) const
        {
            std::format_to(out, "ERROR-EXPRESSION");
        }
    };

    template <class Out>
    struct Pattern_format_visitor {
        Out          out;
        Arena const& arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(arena), std::cref(x) };
        }

        auto operator()(kieli::literal auto const& literal) const
        {
            std::format_to(out, "{}", literal);
        }

        auto operator()(hir::pattern::Wildcard const&) const
        {
            std::format_to(out, "_");
        }

        auto operator()(hir::pattern::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.field_patterns));
        }

        auto operator()(hir::pattern::Slice const& slice) const
        {
            std::format_to(out, "[{}]", wrap(slice.patterns));
        }

        auto operator()(hir::pattern::Name const& name) const
        {
            std::format_to(out, "{} {}", wrap(name.mutability), name.identifier);
        }

        auto operator()(hir::pattern::Alias const& alias) const
        {
            std::format_to(
                out, "{} as {} {}", wrap(alias.pattern), wrap(alias.mutability), alias.identifier);
        }

        auto operator()(hir::pattern::Guarded const& guarded) const
        {
            std::format_to(
                out, "{} if {}", wrap(guarded.guarded_pattern), wrap(guarded.guard_expression));
        }
    };

    template <class Out>
    struct Type_format_visitor {
        Out          out;
        Arena const& arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(arena), std::cref(x) };
        }

        auto operator()(kieli::type::Integer const integer) const
        {
            std::format_to(out, "{}", kieli::type::integer_name(integer));
        }

        auto operator()(kieli::type::Floating const&) const
        {
            std::format_to(out, "Float");
        }

        auto operator()(kieli::type::Character const&) const
        {
            std::format_to(out, "Char");
        }

        auto operator()(kieli::type::Boolean const&) const
        {
            std::format_to(out, "Bool");
        }

        auto operator()(kieli::type::String const&) const
        {
            std::format_to(out, "String");
        }

        auto operator()(hir::type::Array const& array) const
        {
            std::format_to(
                out,
                "[{}; {}]",
                wrap(std::cref(array.element_type)),
                wrap(std::cref(array.length)));
        }

        auto operator()(hir::type::Slice const& slice) const
        {
            std::format_to(out, "[{}]", wrap(slice.element_type));
        }

        auto operator()(hir::type::Reference const& reference) const
        {
            std::format_to(
                out, "&{} {}", wrap(reference.mutability), wrap(reference.referenced_type));
        }

        auto operator()(hir::type::Pointer const& pointer) const
        {
            std::format_to(out, "*{} {}", wrap(pointer.mutability), wrap(pointer.pointee_type));
        }

        auto operator()(hir::type::Function const& function) const
        {
            std::format_to(
                out, "fn({}): {}", wrap(function.parameter_types), wrap(function.return_type));
        }

        auto operator()(hir::type::Enumeration const& enumeration) const
        {
            std::format_to(out, "{}", enumeration.name);
        }

        auto operator()(hir::type::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.types));
        }

        auto operator()(hir::type::Parameterized const& parameterized) const
        {
            std::format_to(out, "template-parameter-{}", parameterized.tag.get());
        }

        auto operator()(hir::type::Variable const& variable) const
        {
            std::format_to(out, "?{}", variable.id.get());
        }

        auto operator()(hir::type::Error const&) const
        {
            std::format_to(out, "ERROR-TYPE");
        }
    };
} // namespace kieli::hir::dtl

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Expression_variant)
{
    kieli::hir::dtl::Expression_format_visitor visitor { context.out(), value.arena.get() };
    std::visit(visitor, value.get());
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Expression_id)
{
    return std::format_to(
        context.out(), "{}", value.wrap(value.arena.get().expressions[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Expression)
{
    return std::format_to(
        context.out(), "({}: {})", value.wrap(value->variant), value.wrap(value->type));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Pattern_variant)
{
    kieli::hir::dtl::Pattern_format_visitor visitor { context.out(), value.arena.get() };
    std::visit(visitor, value.get());
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Pattern_id)
{
    return std::format_to(context.out(), "{}", value.wrap(value.arena.get().patterns[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Pattern)
{
    return std::format_to(
        context.out(), "({}: {})", value.wrap(value->variant), value.wrap(value->type));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Type_variant)
{
    kieli::hir::dtl::Type_format_visitor visitor { context.out(), value.arena.get() };
    std::visit(visitor, value.get());
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Type_id)
{
    return std::format_to(context.out(), "{}", value.wrap(value.arena.get().types[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Type)
{
    return std::format_to(context.out(), "{}", value.wrap(value->id));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Mutability_variant)
{
    return std::visit(
        utl::Overload {
            [&](kieli::hir::mutability::Concrete const concrete) {
                return std::format_to(context.out(), "{}", concrete);
            },
            [&](kieli::hir::mutability::Parameterized const& parameterized) {
                return std::format_to(context.out(), "mut?{}", parameterized.tag.get());
            },
            [&](kieli::hir::mutability::Error const&) {
                return std::format_to(context.out(), "mut?ERROR");
            },
            [&](kieli::hir::mutability::Variable const& variable) {
                return std::format_to(context.out(), "?mut{}", variable.id.get());
            },
        },
        value.get());
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Mutability_id)
{
    return std::format_to(
        context.out(), "{}", value.wrap(value.arena.get().mutabilities[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Mutability)
{
    return std::format_to(context.out(), "{}", value.wrap(value->id));
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Function_parameter)
{
    std::format_to(context.out(), "{}: {}", value.wrap(value->pattern), value.wrap(value->type));
    if (value->default_argument.has_value()) {
        std::format_to(context.out(), " = {}", value.wrap(value->default_argument.value()));
    }
    return context.out();
}

LIBRESOLVE_DEFINE_FORMATTER(kieli::hir::Function_argument)
{
    if (value->name.has_value()) {
        std::format_to(context.out(), "{} = ", value->name.value());
    }
    return std::format_to(context.out(), "{}", value.wrap(value->expression));
}

#undef LIBRESOLVE_DECLARE_FORMATTER
#undef LIBRESOLVE_DEFINE_FORMATTER
