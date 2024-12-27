#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace libresolve;

namespace {
    enum struct Goal { equality, subtype };
    enum struct Result { ok, mismatch, recursive };

    auto result(bool result) -> Result
    {
        return result ? Result::ok : Result::mismatch;
    }

    auto bind(Result result, std::invocable auto callback) -> Result
    {
        return result != Result::ok ? result : std::invoke(std::move(callback));
    }

    struct Mutability_visitor {
        Context&               context;
        Inference_state&       state;
        hir::Mutability const& current_sub;
        hir::Mutability const& current_super;
        Goal                   goal {};

        auto solution(hir::Mutability_variable_id var_id, hir::Mutability mut) const -> Result
        {
            set_solution(
                context,
                state,
                state.mutability_variables[var_id],
                context.hir.mutabilities[mut.id]);
            return Result::ok;
        }

        auto unify(hir::Mutability const sub, hir::Mutability const super) const -> Result
        {
            Mutability_visitor visitor { context, state, sub, super, goal };
            return std::visit(
                visitor, context.hir.mutabilities[sub.id], context.hir.mutabilities[super.id]);
        }

        auto operator()(kieli::Mutability const sub, kieli::Mutability const super) const -> Result
        {
            return result(
                (sub == super) or (sub == kieli::Mutability::mut and goal == Goal::subtype));
        }

        auto operator()(hir::mutability::Variable const sub, hir::mutability::Variable const super)
            const -> Result
        {
            if (sub.id != super.id) {
                state.mutability_variable_disjoint_set.merge(sub.id.get(), super.id.get());
            }
            return Result::ok;
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
            return Result::mismatch;
        }
    };

    struct Type_visitor {
        Context&                 context;
        Inference_state&         state;
        hir::Type_variant const& current_sub;
        hir::Type_variant const& current_super;
        Goal                     goal {};

        auto solution(hir::Type_variable_id const tag, hir::Type_variant type) const -> Result
        {
            if (occurs_check(context.hir, tag, type)) {
                return Result::recursive;
            }
            flatten_type(context, state, type);
            set_solution(context, state, state.type_variables[tag], std::move(type));
            return Result::ok;
        }

        auto unify(hir::Type_variant const& sub, hir::Type_variant const& super) const -> Result
        {
            Type_visitor visitor { context, state, sub, super, goal };
            return std::visit(visitor, sub, super);
        }

        auto unify(hir::Type const sub, hir::Type const super) const -> Result
        {
            return unify(context.hir.types[sub.id], context.hir.types[super.id]);
        }

        auto unify(hir::Mutability const sub, hir::Mutability const super) const -> Result
        {
            Mutability_visitor visitor { context, state, sub, super, goal };
            return visitor.unify(sub, super);
        }

        auto unify(std::span<hir::Type const> sub, std::span<hir::Type const> super) const -> Result
        {
            if (sub.size() != super.size()) {
                return Result::mismatch;
            }
            for (auto const& [sub, super] : std::views::zip(sub, super)) {
                if (auto result = unify(sub, super); result != Result::ok) {
                    return result;
                }
            }
            return Result::ok;
        }

        auto operator()(hir::type::Variable const sub, hir::type::Variable const super) const
            -> Result
        {
            // TODO: handle integrals
            if (sub.id != super.id) {
                state.type_variable_disjoint_set.merge(sub.id.get(), super.id.get());
            }
            return Result::ok;
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
            return Result::ok;
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
                    hir::expression_type(context.hir.expressions[sub.length]),
                    hir::expression_type(context.hir.expressions[super.length]));
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

        auto operator()(hir::Error, hir::Error) const -> Result
        {
            return Result::ok;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(hir::Error, T const&) const -> Result
        {
            return Result::ok;
        }

        template <typename T>
            requires(not std::is_same_v<T, hir::type::Variable>)
        auto operator()(T const&, hir::Error) const -> Result
        {
            return Result::ok;
        }

        auto operator()(auto const&, auto const&) const -> Result
        {
            return Result::mismatch;
        }
    };
} // namespace

void libresolve::require_subtype_relationship(
    Context&                 context,
    Inference_state&         state,
    kieli::Range const       range,
    hir::Type_variant const& sub,
    hir::Type_variant const& super)
{
    auto const visitor = Type_visitor { context, state, sub, super, Goal::subtype };
    auto const result  = visitor.unify(sub, super);

    if (result != Result::ok) {
        auto const left  = hir::to_string(context.hir, sub);
        auto const right = hir::to_string(context.hir, super);

        auto const description
            = result == Result::recursive ? "Recursive type variable solution" : "Could not unify";

        auto message = std::format("{} {} ~> {}", description, left, right);
        kieli::add_error(context.db, state.document_id, range, std::move(message));
    }
}
