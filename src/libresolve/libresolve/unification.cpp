#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

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

    void merge_variable_kinds(hir::Type_variable_kind& lhs, hir::Type_variable_kind& rhs)
    {
        // TODO: handle concept requirements
        if (lhs == hir::Type_variable_kind::Integral) {
            rhs = hir::Type_variable_kind::Integral;
        }
        else if (rhs == hir::Type_variable_kind::Integral) {
            lhs = hir::Type_variable_kind::Integral;
        }
    }

    struct Mutability_visitor {
        db::Database&                  db;
        Context&                       ctx;
        Block_state&                   state;
        hir::Mutability_variant const& current_sub;
        hir::Mutability_variant const& current_super;
        Goal                           goal {};

        auto solution(hir::Mutability_variable_id var_id, hir::Mutability_variant const& solution)
            const -> Result
        {
            set_mut_solution(db, ctx, state, var_id, solution);
            return Result::Ok;
        }

        auto unify(hir::Mutability sub, hir::Mutability super) const -> Result
        {
            Mutability_visitor visitor {
                .db            = db,
                .ctx           = ctx,
                .state         = state,
                .current_sub   = ctx.arena.hir.mutabilities[sub.id],
                .current_super = ctx.arena.hir.mutabilities[super.id],
                .goal          = goal,
            };
            return std::visit(
                visitor, ctx.arena.hir.mutabilities[sub.id], ctx.arena.hir.mutabilities[super.id]);
        }

        auto operator()(db::Mutability sub, db::Mutability super) const -> Result
        {
            return result((sub == super) or (sub == db::Mutability::Mut and goal == Goal::Subtype));
        }

        auto operator()(db::Mutability sub, hir::mut::Parameterized) const -> Result
        {
            return result(sub == db::Mutability::Mut and goal == Goal::Subtype);
        }

        auto operator()(hir::mut::Parameterized, db::Mutability super) const -> Result
        {
            return result(super == db::Mutability::Immut and goal == Goal::Subtype);
        }

        auto operator()(hir::mut::Variable sub, hir::mut::Variable super) const -> Result
        {
            if (sub.id != super.id) {
                state.mut_var_set.merge(sub.id.get(), super.id.get());
            }
            return Result::Ok;
        }

        auto operator()(hir::mut::Variable variable, auto const&) const -> Result
        {
            return solution(variable.id, current_super);
        }

        auto operator()(auto const&, hir::mut::Variable variable) const -> Result
        {
            return solution(variable.id, current_sub);
        }

        auto operator()(hir::mut::Parameterized sub, hir::mut::Parameterized super) const -> Result
        {
            return result(sub.tag.value == super.tag.value);
        }

        auto operator()(auto const&, auto const&) const -> Result
        {
            return Result::Mismatch;
        }
    };

    struct Type_visitor {
        db::Database&            db;
        Context&                 ctx;
        Block_state&             state;
        hir::Type_variant const& current_sub;
        hir::Type_variant const& current_super;
        Goal                     goal {};

        auto solution(hir::Type_variable_id var_id, hir::Type_variant type) const -> Result
        {
            if (occurs_check(ctx.arena.hir, var_id, type)) {
                return Result::Recursive;
            }
            flatten_type(ctx, state, type);
            set_type_solution(db, ctx, state, var_id, std::move(type));
            return Result::Ok;
        }

        auto unify(hir::Type_variant const& sub, hir::Type_variant const& super) const -> Result
        {
            Type_visitor visitor {
                .db            = db,
                .ctx           = ctx,
                .state         = state,
                .current_sub   = sub,
                .current_super = super,
                .goal          = goal,
            };
            return std::visit(visitor, sub, super);
        }

        auto unify(hir::Type_id sub, hir::Type_id super) const -> Result
        {
            return unify(ctx.arena.hir.types[sub], ctx.arena.hir.types[super]);
        }

        auto unify(hir::Mutability const sub, hir::Mutability const super) const -> Result
        {
            Mutability_visitor visitor {
                .db            = db,
                .ctx           = ctx,
                .state         = state,
                .current_sub   = ctx.arena.hir.mutabilities[sub.id],
                .current_super = ctx.arena.hir.mutabilities[super.id],
                .goal          = goal,
            };
            return visitor.unify(sub, super);
        }

        auto unify(std::span<hir::Type_id const> sub, std::span<hir::Type_id const> super) const
            -> Result
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

        auto operator()(hir::type::Variable sub, hir::type::Variable super) const -> Result
        {
            if (sub.id != super.id) {
                merge_variable_kinds(
                    state.type_vars.at(sub.id.get()).kind, state.type_vars.at(super.id.get()).kind);
                state.type_var_set.merge(sub.id.get(), super.id.get());
            }
            return Result::Ok;
        }

        auto try_solve(hir::type::Variable variable, hir::Type_variant const& variant) const
            -> Result
        {
            if (state.type_vars.at(variable.id.get()).kind == hir::Type_variable_kind::Integral) {
                if (auto const* builtin = std::get_if<hir::type::Builtin>(&variant)) {
                    if (hir::is_integral(*builtin)) {
                        return solution(variable.id, variant);
                    }
                }
                return Result::Mismatch;
            }
            return solution(variable.id, variant);
        }

        auto operator()(hir::type::Variable sub, auto const&) const -> Result
        {
            return try_solve(sub, current_super);
        }

        auto operator()(auto const&, hir::type::Variable super) const -> Result
        {
            return try_solve(super, current_sub);
        }

        auto operator()(hir::type::Builtin sub, hir::type::Builtin super) const -> Result
        {
            return result(sub == super or sub == hir::type::Builtin::Never);
        }

        template <typename T>
            requires(not utl::one_of<T, db::Error>)
        auto operator()(hir::type::Builtin sub, T const&) const -> Result
        {
            return result(sub == hir::type::Builtin::Never);
        }

        auto operator()(hir::type::Builtin sub, hir::type::Variable super) const -> Result
        {
            if (sub == hir::type::Builtin::Never) {
                return Result::Ok;
            }
            return try_solve(super, sub);
        }

        auto operator()(hir::type::Parameterized const& sub, hir::type::Parameterized const& super)
            const -> Result
        {
            return result(sub.tag.value == super.tag.value);
        }

        auto operator()(hir::type::Tuple const& sub, hir::type::Tuple const& super) const -> Result
        {
            return unify(sub.types, super.types);
        }

        auto operator()(hir::type::Array const& sub, hir::type::Array const& super) const -> Result
        {
            return bind(unify(sub.element_type, super.element_type), [&] {
                return unify(
                    ctx.arena.hir.expressions[sub.length].type_id,
                    ctx.arena.hir.expressions[super.length].type_id);
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

        template <utl::one_of<hir::type::Structure, hir::type::Enumeration> T>
        auto operator()(T const& sub, T const& super) const -> Result
        {
            // TODO: visit template arguments
            return result(sub.id == super.id);
        }

        auto operator()(db::Error, db::Error) const -> Result
        {
            return Result::Ok;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(db::Error, T const&) const -> Result
        {
            return Result::Ok;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(T const&, db::Error) const -> Result
        {
            return Result::Ok;
        }

        auto operator()(auto const&, auto const&) const -> Result
        {
            return Result::Mismatch;
        }
    };
} // namespace

void ki::res::require_subtype_relationship(
    db::Database&            db,
    Context&                 ctx,
    Block_state&             state,
    lsp::Range const         range,
    hir::Type_variant const& sub,
    hir::Type_variant const& super)
{
    Type_visitor visitor {
        .db            = db,
        .ctx           = ctx,
        .state         = state,
        .current_sub   = sub,
        .current_super = super,
        .goal          = Goal::Subtype,
    };
    Result result = visitor.unify(sub, super);

    if (result != Result::Ok) {
        auto const left  = hir::to_string(ctx.arena.hir, db.string_pool, sub);
        auto const right = hir::to_string(ctx.arena.hir, db.string_pool, super);

        char const* const description
            = result == Result::Recursive ? "Recursive type variable solution" : "Could not unify";

        ctx.add_diagnostic(lsp::error(range, std::format("{} {} ~> {}", description, left, right)));
    }
}

void ki::res::require_submutability_relationship(
    db::Database&                  db,
    Context&                       ctx,
    Block_state&                   state,
    lsp::Range                     range,
    hir::Mutability_variant const& sub,
    hir::Mutability_variant const& super)
{
    Mutability_visitor visitor {
        .db            = db,
        .ctx           = ctx,
        .state         = state,
        .current_sub   = sub,
        .current_super = super,
        .goal          = Goal::Subtype,
    };

    if (std::visit(visitor, sub, super) != Result::Ok) {
        auto const left  = hir::to_string(ctx.arena.hir, db.string_pool, sub);
        auto const right = hir::to_string(ctx.arena.hir, db.string_pool, super);
        ctx.add_diagnostic(lsp::error(range, std::format("Could not unify {} ~> {}", left, right)));
    }
}
