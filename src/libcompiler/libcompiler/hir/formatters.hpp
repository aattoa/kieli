#ifndef KIELI_LIBCOMPILER_HIR_FORMATTERS
#define KIELI_LIBCOMPILER_HIR_FORMATTERS

#include <libutl/utilities.hpp>
#include <libcompiler/hir/hir.hpp>

// TODO: simplify

namespace ki::hir::dtl {
    template <typename T>
    struct With_arena {
        std::reference_wrapper<utl::String_pool const> pool;
        std::reference_wrapper<Arena const>            arena;
        std::reference_wrapper<T const>                object;

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
            return { pool, arena, std::cref(other) };
        }
    };
} // namespace ki::hir::dtl

#define LIBRESOLVE_DECLARE_FORMATTER(...)                                    \
    template <>                                                              \
    struct std::formatter<ki::hir::dtl::With_arena<__VA_ARGS__>> {           \
        static constexpr auto parse(auto& ctx)                               \
        {                                                                    \
            return ctx.begin();                                              \
        }                                                                    \
        static auto format(ki::hir::dtl::With_arena<__VA_ARGS__>, auto& ctx) \
            -> decltype(ctx.out());                                          \
    }

#define LIBRESOLVE_DEFINE_FORMATTER(...)                                \
    auto std::formatter<ki::hir::dtl::With_arena<__VA_ARGS__>>::format( \
        ki::hir::dtl::With_arena<__VA_ARGS__> value, auto& ctx) -> decltype(ctx.out())

LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Pattern_variant);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Pattern_id);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Pattern);

LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Expression_variant);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Expression_id);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Expression);

LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Type_variant);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Type_id);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Type);

LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Mutability_variant);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Mutability_id);
LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Mutability);

LIBRESOLVE_DECLARE_FORMATTER(ki::hir::Function_parameter);

template <typename T>
    requires std::formattable<ki::hir::dtl::With_arena<T>, char>
struct std::formatter<ki::hir::dtl::With_arena<std::vector<T>>> // NOLINT(cert-dcl58-cpp)
{
    static constexpr auto parse(auto& ctx)
    {
        return ctx.begin();
    }

    static auto format(ki::hir::dtl::With_arena<std::vector<T>> const value, auto& ctx)
        -> decltype(ctx.out())
    {
        auto const wrap = [&](auto const& x) { return value.wrap(x); };
        return std::format_to(ctx.out(), "{:n}", std::views::transform(value.object.get(), wrap));
    }
};

namespace ki::hir::dtl {
    template <typename Out>
    struct Expression_format_visitor {
        Out                     out;
        utl::String_pool const& pool;
        Arena const&            arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(pool), std::cref(arena), std::cref(x) };
        }

        void operator()(hir::Wildcard const&) const
        {
            std::format_to(out, "_");
        }

        void operator()(utl::one_of<db::Integer, db::Floating, db::Boolean> auto literal)
        {
            std::format_to(out, "{}", literal.value);
        }

        void operator()(db::String const& literal) const
        {
            std::format_to(out, "{:?}", pool.get(literal.id));
        }

        void operator()(expr::Array const& literal) const
        {
            std::format_to(out, "[{}]", wrap(literal.elements));
        }

        void operator()(expr::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.fields));
        }

        void operator()(expr::Loop const& loop) const
        {
            std::format_to(out, "loop {}", wrap(loop.body));
        }

        void operator()(expr::Break const& break_) const
        {
            std::format_to(out, "break {}", wrap(break_.result));
        }

        void operator()(expr::Continue const&) const
        {
            std::format_to(out, "continue");
        }

        void operator()(expr::Block const& block) const
        {
            std::format_to(out, "{{");
            for (Expression const& side_effect : block.effects) {
                std::format_to(out, " {};", wrap(side_effect));
            }
            std::format_to(out, " {} }}", wrap(block.result));
        }

        void operator()(expr::Let const& let) const
        {
            std::format_to(
                out, "let {}: {} = {}", wrap(let.pattern), wrap(let.type), wrap(let.initializer));
        }

        void operator()(expr::Match const& match) const
        {
            std::format_to(out, "match {} {{", wrap(match.scrutinee));
            for (auto const& arm : match.arms) {
                std::format_to(out, " {} -> {}", wrap(arm.pattern), wrap(arm.expression));
            }
            std::format_to(out, " }}");
        }

        void operator()(expr::Variable_reference const& variable) const
        {
            std::format_to(out, "{}", pool.get(arena.local_variables[variable.id].name.id));
        }

        void operator()(expr::Function_reference const& reference) const
        {
            std::format_to(out, "{}", pool.get(arena.functions[reference.id].name.id));
        }

        void operator()(expr::Constructor_reference const& reference) const
        {
            std::format_to(out, "{}", pool.get(arena.constructors[reference.id].name.id));
        }

        void operator()(expr::Function_call const& call) const
        {
            std::format_to(out, "{}({})", wrap(call.invocable), wrap(call.arguments));
        }

        void operator()(expr::Initializer const& init) const
        {
            std::format_to(out, "{}(..)", pool.get(arena.constructors[init.constructor].name.id));
        }

        void operator()(expr::Tuple_field const& field) const
        {
            std::format_to(out, "{}.{}", wrap(field.base), field.index);
        }

        void operator()(expr::Struct_field const& field) const
        {
            std::format_to(
                out, "{}.{}", wrap(field.base), pool.get(arena.fields[field.id].name.id));
        }

        void operator()(expr::Return const& ret) const
        {
            std::format_to(out, "ret {}", wrap(ret.result));
        }

        void operator()(expr::Sizeof const& sizeof_) const
        {
            std::format_to(out, "sizeof({})", wrap(sizeof_.inspected_type));
        }

        void operator()(expr::Addressof const& addressof) const
        {
            std::format_to(out, "(&{} {})", wrap(addressof.mutability), wrap(addressof.expression));
        }

        void operator()(expr::Deref const& dereference) const
        {
            std::format_to(out, "(*{})", wrap(dereference.expression));
        }

        void operator()(expr::Defer const& defer) const
        {
            std::format_to(out, "defer {}", wrap(defer.expression));
        }

        void operator()(db::Error const&) const
        {
            std::format_to(out, "ERROR-EXPRESSION");
        }
    };

    template <typename Out>
    struct Pattern_format_visitor {
        Out                     out;
        utl::String_pool const& pool;
        Arena const&            arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(pool), std::cref(arena), std::cref(x) };
        }

        void operator()(utl::one_of<db::Integer, db::Floating, db::Boolean> auto literal)
        {
            std::format_to(out, "{}", literal.value);
        }

        void operator()(db::String const& literal) const
        {
            std::format_to(out, "{:?}", pool.get(literal.id));
        }

        void operator()(Wildcard const&) const
        {
            std::format_to(out, "_");
        }

        void operator()(patt::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.field_patterns));
        }

        void operator()(patt::Slice const& slice) const
        {
            std::format_to(out, "[{}]", wrap(slice.patterns));
        }

        void operator()(patt::Name const& name) const
        {
            if (auto const* mut = std::get_if<db::Mutability>(&arena.mutabilities[name.mut_id])) {
                if (*mut == db::Mutability::Immut) {
                    std::format_to(out, "{}", pool.get(name.name_id));
                    return;
                }
            }
            std::format_to(out, "{} {}", wrap(name.mut_id), pool.get(name.name_id));
        }

        void operator()(patt::Guarded const& guarded) const
        {
            std::format_to(out, "{} if {}", wrap(guarded.pattern), wrap(guarded.guard));
        }
    };

    template <typename Out>
    struct Type_format_visitor {
        Out                     out;
        utl::String_pool const& pool;
        Arena const&            arena;

        auto wrap(auto const& x) const
        {
            return With_arena { std::cref(pool), std::cref(arena), std::cref(x) };
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

        void operator()(type::Structure const& structure) const
        {
            std::format_to(out, "{}", pool.get(structure.name.id));
        }

        void operator()(type::Enumeration const& enumeration) const
        {
            std::format_to(out, "{}", pool.get(enumeration.name.id));
        }

        void operator()(type::Tuple const& tuple) const
        {
            std::format_to(out, "({})", wrap(tuple.types));
        }

        void operator()(type::Parameterized const& param) const
        {
            std::format_to(out, "{}", pool.get(param.id));
        }

        void operator()(type::Variable const& variable) const
        {
            std::format_to(out, "?{}", variable.id.get());
        }

        void operator()(db::Error const&) const
        {
            std::format_to(out, "ERROR-TYPE");
        }
    };
} // namespace ki::hir::dtl

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Expression_variant)
{
    ki::hir::dtl::Expression_format_visitor visitor {
        ctx.out(),
        value.pool.get(),
        value.arena.get(),
    };
    std::visit(visitor, value.get());
    return ctx.out();
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Expression_id)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value.arena.get().expressions[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Expression)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value->variant));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Pattern_variant)
{
    ki::hir::dtl::Pattern_format_visitor visitor {
        ctx.out(),
        value.pool.get(),
        value.arena.get(),
    };
    std::visit(visitor, value.get());
    return ctx.out();
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Pattern_id)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value.arena.get().patterns[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Pattern)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value->variant));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Type_variant)
{
    ki::hir::dtl::Type_format_visitor visitor {
        ctx.out(),
        value.pool.get(),
        value.arena.get(),
    };
    std::visit(visitor, value.get());
    return ctx.out();
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Type_id)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value.arena.get().types[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Type)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value->id));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Mutability_variant)
{
    auto const visitor = ki::utl::Overload {
        [&](ki::db::Error) {
            return std::format_to(ctx.out(), "mut?ERROR"); //
        },
        [&](ki::db::Mutability concrete) {
            return std::format_to(ctx.out(), "{}", ki::db::mutability_string(concrete));
        },
        [&](ki::hir::mut::Parameterized const& parameterized) {
            return std::format_to(ctx.out(), "mut?{}", parameterized.tag.value);
        },
        [&](ki::hir::mut::Variable const& variable) {
            return std::format_to(ctx.out(), "?mut{}", variable.id.get());
        },
    };
    return std::visit(visitor, value.get());
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Mutability_id)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value.arena.get().mutabilities[value.get()]));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Mutability)
{
    return std::format_to(ctx.out(), "{}", value.wrap(value->id));
}

LIBRESOLVE_DEFINE_FORMATTER(ki::hir::Function_parameter)
{
    std::format_to(ctx.out(), "{}: {}", value.wrap(value->pattern_id), value.wrap(value->type.id));
    if (value->default_argument.has_value()) {
        std::format_to(ctx.out(), " = {}", value.wrap(value->default_argument.value()));
    }
    return ctx.out();
}

#undef LIBRESOLVE_DECLARE_FORMATTER
#undef LIBRESOLVE_DEFINE_FORMATTER

#endif // KIELI_LIBCOMPILER_HIR_FORMATTERS
