#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/hir/hir.hpp>

// TODO: simplify

namespace kieli::hir::dtl {
    template <typename T>
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

        template <typename Other>
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

template <typename T>
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
    template <typename Out>
    struct Expression_format_visitor {
        Out          out;
        Arena const& arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(arena), std::cref(x) };
        }

        void operator()(hir::Wildcard const&) const
        {
            std::format_to(out, "_");
        }

        void operator()(kieli::literal auto const& literal) const
        {
            std::format_to(out, "{}", literal);
        }

        void operator()(expression::Array_literal const& literal) const
        {
            std::format_to(out, "[{}]", wrap(literal.elements));
        }

        void operator()(expression::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.fields));
        }

        void operator()(expression::Loop const& loop) const
        {
            std::format_to(out, "loop {}", wrap(loop.body));
        }

        void operator()(expression::Break const& break_) const
        {
            std::format_to(out, "break {}", wrap(break_.result));
        }

        void operator()(expression::Continue const&) const
        {
            std::format_to(out, "continue");
        }

        void operator()(expression::Block const& block) const
        {
            std::format_to(out, "{{");
            for (Expression const& side_effect : block.side_effects) {
                std::format_to(out, " {};", wrap(side_effect));
            }
            std::format_to(out, " {} }}", wrap(block.result));
        }

        void operator()(expression::Let_binding const& let) const
        {
            std::format_to(
                out, "let {}: {} = {}", wrap(let.pattern), wrap(let.type), wrap(let.initializer));
        }

        void operator()(expression::Match const& match) const
        {
            std::format_to(out, "match {} {{", wrap(match.expression));
            for (auto const& match_case : match.cases) {
                std::format_to(
                    out, " {} -> {}", wrap(match_case.pattern), wrap(match_case.expression));
            }
            std::format_to(out, " }}");
        }

        void operator()(expression::Variable_reference const& variable) const
        {
            std::format_to(out, "{}", variable.name);
        }

        void operator()(expression::Function_reference const& reference) const
        {
            std::format_to(out, "{}", reference.name);
        }

        void operator()(expression::Indirect_function_call const& call) const
        {
            std::format_to(out, "{}({})", wrap(call.invocable), wrap(call.arguments));
        }

        void operator()(expression::Direct_function_call const& call) const
        {
            std::format_to(out, "{}({})", call.function_name, wrap(call.arguments));
        }

        void operator()(expression::Sizeof const& sizeof_) const
        {
            std::format_to(out, "sizeof({})", wrap(sizeof_.inspected_type));
        }

        void operator()(expression::Addressof const& addressof) const
        {
            std::format_to(
                out, "(&{} {})", wrap(addressof.mutability), wrap(addressof.place_expression));
        }

        void operator()(expression::Dereference const& dereference) const
        {
            std::format_to(out, "(*{})", wrap(dereference.reference_expression));
        }

        void operator()(expression::Defer const& defer) const
        {
            std::format_to(out, "defer {}", wrap(defer.effect_expression));
        }

        void operator()(Error const&) const
        {
            std::format_to(out, "ERROR-EXPRESSION");
        }
    };

    template <typename Out>
    struct Pattern_format_visitor {
        Out          out;
        Arena const& arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(arena), std::cref(x) };
        }

        void operator()(kieli::literal auto const& literal) const
        {
            std::format_to(out, "{}", literal);
        }

        void operator()(Wildcard const&) const
        {
            std::format_to(out, "_");
        }

        void operator()(pattern::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.field_patterns));
        }

        void operator()(pattern::Slice const& slice) const
        {
            std::format_to(out, "[{}]", wrap(slice.patterns));
        }

        void operator()(pattern::Name const& name) const
        {
            std::format_to(out, "{} {}", wrap(name.mutability), name.identifier);
        }

        void operator()(pattern::Alias const& alias) const
        {
            std::format_to(
                out, "{} as {} {}", wrap(alias.pattern), wrap(alias.mutability), alias.identifier);
        }

        void operator()(pattern::Guarded const& guarded) const
        {
            std::format_to(
                out, "{} if {}", wrap(guarded.guarded_pattern), wrap(guarded.guard_expression));
        }
    };

    template <typename Out>
    struct Type_format_visitor {
        Out          out;
        Arena const& arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(arena), std::cref(x) };
        }

        void operator()(type::Integer const integer) const
        {
            std::format_to(out, "{}", integer_name(integer));
        }

        void operator()(type::Floating const&) const
        {
            std::format_to(out, "Float");
        }

        void operator()(type::Character const&) const
        {
            std::format_to(out, "Char");
        }

        void operator()(type::Boolean const&) const
        {
            std::format_to(out, "Bool");
        }

        void operator()(type::String const&) const
        {
            std::format_to(out, "String");
        }

        void operator()(type::Array const& array) const
        {
            std::format_to(
                out,
                "[{}; {}]",
                wrap(std::cref(array.element_type)),
                wrap(std::cref(array.length)));
        }

        void operator()(type::Slice const& slice) const
        {
            std::format_to(out, "[{}]", wrap(slice.element_type));
        }

        void operator()(type::Reference const& reference) const
        {
            std::format_to(
                out, "&{} {}", wrap(reference.mutability), wrap(reference.referenced_type));
        }

        void operator()(type::Pointer const& pointer) const
        {
            std::format_to(out, "*{} {}", wrap(pointer.mutability), wrap(pointer.pointee_type));
        }

        void operator()(type::Function const& function) const
        {
            std::format_to(
                out, "fn({}): {}", wrap(function.parameter_types), wrap(function.return_type));
        }

        void operator()(type::Enumeration const& enumeration) const
        {
            std::format_to(out, "{}", enumeration.name);
        }

        void operator()(type::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.types));
        }

        void operator()(type::Parameterized const& parameterized) const
        {
            std::format_to(out, "template-parameter-{}", parameterized.tag.get());
        }

        void operator()(type::Variable const& variable) const
        {
            std::format_to(out, "?{}", variable.id.get());
        }

        void operator()(Error const&) const
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
            [&](kieli::hir::Error const&) {
                return std::format_to(context.out(), "mut?ERROR"); //
            },
            [&](kieli::Mutability const concrete) {
                return std::format_to(context.out(), "{}", concrete);
            },
            [&](kieli::hir::mutability::Parameterized const& parameterized) {
                return std::format_to(context.out(), "mut?{}", parameterized.tag.get());
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

#undef LIBRESOLVE_DECLARE_FORMATTER
#undef LIBRESOLVE_DEFINE_FORMATTER
