#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    enum struct Goal : std::uint8_t { Equality, Subtype };
    enum struct Result : std::uint8_t { Ok, Mismatch, Recursive };

    auto result(bool result) -> Result
    {
        return result ? Result::Ok : Result::Mismatch;
    }

    auto bind(Result result, std::invocable auto callback) -> Result
    {
        return result != Result::Ok ? result : std::invoke(std::move(callback));
    }

    struct Mutability_visitor {
        Context&               ctx;
        Inference_state&       state;
        hir::Mutability const& current_sub;
        hir::Mutability const& current_super;
        Goal                   goal {};

        [[nodiscard]] auto solution(hir::Mutability_variable_id var_id, hir::Mutability mut) const
            -> Result
        {
            set_solution(ctx, state, state.mut_vars[var_id], ctx.hir.mut[mut.id]);
            return Result::Ok;
        }

        [[nodiscard]] auto unify(hir::Mutability const sub, hir::Mutability const super) const
            -> Result
        {
            Mutability_visitor visitor {
                .ctx           = ctx,
                .state         = state,
                .current_sub   = sub,
                .current_super = super,
                .goal          = goal,
            };
            return std::visit(visitor, ctx.hir.mut[sub.id], ctx.hir.mut[super.id]);
        }

        auto operator()(ki::Mutability const sub, ki::Mutability const super) const -> Result
        {
            return result((sub == super) or (sub == ki::Mutability::Mut and goal == Goal::Subtype));
        }

        auto operator()(hir::mutability::Variable const sub, hir::mutability::Variable const super)
            const -> Result
        {
            if (sub.id != super.id) {
                state.mut_var_set.merge(sub.id.get(), super.id.get());
            }
            return Result::Ok;
        }

        auto operator()(hir::mutability::Variable const variable, auto const&) const -> Result
        {
            return solution(variable.id, current_super);
        }

        auto operator()(auto const&, hir::mutability::Variable const variable) const -> Result
        {
            return solution(variable.id, current_sub);
        }

        auto operator()(
            hir::mutability::Parameterized const sub,
            hir::mutability::Parameterized const super) const -> Result
        {
            return result(sub.tag.get() == super.tag.get());
        }

        auto operator()(auto const&, auto const&) const -> Result
        {
            return Result::Mismatch;
        }
    };

    struct Type_visitor {
        Context&                 ctx;
        Inference_state&         state;
        hir::Type_variant const& current_sub;
        hir::Type_variant const& current_super;
        Goal                     goal {};

        [[nodiscard]] auto solution(hir::Type_variable_id id, hir::Type_variant type) const
            -> Result
        {
            if (occurs_check(ctx.hir, id, type)) {
                return Result::Recursive;
            }
            flatten_type(ctx, state, type);
            set_solution(ctx, state, state.type_vars[id], std::move(type));
            return Result::Ok;
        }

        [[nodiscard]] auto unify(hir::Type_variant const& sub, hir::Type_variant const& super) const
            -> Result
        {
            Type_visitor visitor {
                .ctx           = ctx,
                .state         = state,
                .current_sub   = sub,
                .current_super = super,
                .goal          = goal,
            };
            return std::visit(visitor, sub, super);
        }

        [[nodiscard]] auto unify(hir::Type const sub, hir::Type const super) const -> Result
        {
            return unify(ctx.hir.type[sub.id], ctx.hir.type[super.id]);
        }

        [[nodiscard]] auto unify(hir::Mutability const sub, hir::Mutability const super) const
            -> Result
        {
            Mutability_visitor visitor {
                .ctx           = ctx,
                .state         = state,
                .current_sub   = sub,
                .current_super = super,
                .goal          = goal,
            };
            return visitor.unify(sub, super);
        }

        [[nodiscard]] auto unify(
            std::span<hir::Type const> sub, std::span<hir::Type const> super) const -> Result
        {
            if (sub.size() != super.size()) {
                return Result::Mismatch;
            }
            for (auto const& [sub, super] : std::views::zip(sub, super)) {
                if (auto result = unify(sub, super); result != Result::Ok) {
                    return result;
                }
            }
            return Result::Ok;
        }

        auto operator()(hir::type::Variable const sub, hir::type::Variable const super) const
            -> Result
        {
            // TODO: handle integrals
            if (sub.id != super.id) {
                state.type_var_set.merge(sub.id.get(), super.id.get());
            }
            return Result::Ok;
        }

        auto operator()(hir::type::Variable const sub, auto const&) const -> Result
        {
            // TODO: handle integrals
            return solution(sub.id, current_super);
        }

        auto operator()(auto const&, hir::type::Variable const super) const -> Result
        {
            // TODO: handle integrals
            return solution(super.id, current_sub);
        }

        template <utl::one_of<
            hir::type::Floating,
            hir::type::Character,
            hir::type::Boolean,
            hir::type::String> T>
        auto operator()(T, T) const -> Result
        {
            return Result::Ok;
        }

        auto operator()(hir::type::Integer const sub, hir::type::Integer const super) const
            -> Result
        {
            return result(sub == super);
        }

        auto operator()(hir::type::Parameterized const& sub, hir::type::Parameterized const& super)
            const -> Result
        {
            return result(sub.tag.get() == super.tag.get());
        }

        auto operator()(hir::type::Tuple const& sub, hir::type::Tuple const& super) const -> Result
        {
            return unify(sub.types, super.types);
        }

        auto operator()(hir::type::Array const& sub, hir::type::Array const& super) const -> Result
        {
            return bind(unify(sub.element_type, super.element_type), [&] {
                return unify(
                    hir::expression_type(ctx.hir.expr[sub.length]),
                    hir::expression_type(ctx.hir.expr[super.length]));
            });
        }

        auto operator()(hir::type::Slice const& sub, hir::type::Slice const& super) const -> Result
        {
            return unify(sub.element_type, super.element_type);
        }

        auto operator()(hir::type::Reference const& sub, hir::type::Reference const& super) const
            -> Result
        {
            return bind(unify(sub.referenced_type, super.referenced_type), [&] {
                return unify(sub.mutability, super.mutability);
            });
        }

        auto operator()(hir::type::Pointer const& sub, hir::type::Pointer const& super) const
            -> Result
        {
            return bind(unify(sub.pointee_type, super.pointee_type), [&] {
                return unify(sub.mutability, super.mutability);
            });
        }

        auto operator()(hir::type::Function const& sub, hir::type::Function const& super) const
            -> Result
        {
            return bind(unify(sub.return_type, super.return_type), [&] {
                return unify(sub.parameter_types, super.parameter_types);
            });
        }

        auto operator()(
            hir::type::Enumeration const& sub, hir::type::Enumeration const& super) const -> Result
        {
            // TODO: visit template arguments
            return result(sub.id == super.id);
        }

        auto operator()(ki::Error, ki::Error) const -> Result
        {
            return Result::Ok;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(ki::Error, T const&) const -> Result
        {
            return Result::Ok;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(T const&, ki::Error) const -> Result
        {
            return Result::Ok;
        }

        auto operator()(auto const&, auto const&) const -> Result
        {
            return Result::Mismatch;
        }
    };
} // namespace

void ki::resolve::require_subtype_relationship(
    Context&                 ctx,
    Inference_state&         state,
    ki::Range const          range,
    hir::Type_variant const& sub,
    hir::Type_variant const& super)
{
    Type_visitor visitor {
        .ctx           = ctx,
        .state         = state,
        .current_sub   = sub,
        .current_super = super,
        .goal          = Goal::Subtype,
    };
    auto const result = visitor.unify(sub, super);

    if (result != Result::Ok) {
        auto const left  = hir::to_string(ctx.hir, ctx.db.string_pool, sub);
        auto const right = hir::to_string(ctx.hir, ctx.db.string_pool, super);

        char const* const description
            = result == Result::Recursive ? "Recursive type variable solution" : "Could not unify";

        auto message = std::format("{} {} ~> {}", description, left, right);
        ki::add_error(ctx.db, state.doc_id, range, std::move(message));
    }
}
