#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    template <typename Bind>
    void do_bind(Identifier_map<Bind>& bindings, Bind bind)
    {
        // Easy way to implement variable shadowing
        auto const it = std::ranges::find(bindings, bind.name.id, utl::first);
        bindings.emplace(it, bind.name.id, std::move(bind));
    }

    template <typename T, Identifier_map<T> Scope::* bindings>
    auto do_find(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id) -> T*
    {
        for (;;) {
            Scope& scope = ctx.info.scopes.index_vector[scope_id];
            if (auto const it = std::ranges::find(scope.*bindings, string_id, utl::first);
                it != (scope.*bindings).end()) {
                return std::addressof(it->second);
            }
            else if (scope.parent_id.has_value()) {
                scope_id = scope.parent_id.value();
            }
            else {
                return nullptr;
            }
        }
    }

    template <typename Binding>
    void do_report_unused_bindings(
        ki::Database& db, ki::Document_id doc_id, Identifier_map<Binding> const& bindings)
    {
        auto const is_unused = [&db](Binding const& binding) {
            return binding.unused and not db.string_pool.get(binding.name.id).starts_with('_');
        };
        auto const warn = [&db](Binding const& binding) {
            std::string_view const name = db.string_pool.get(binding.name.id);
            return ki::warning(
                binding.name.range,
                std::format("Unused binding: {}. If this is intentional, use _{}", name, name));
        };
        auto diagnostics = std::views::values(bindings)  //
                         | std::views::filter(is_unused) //
                         | std::views::transform(warn);
        std::ranges::copy(diagnostics, std::back_inserter(db.documents[doc_id].diagnostics));
    }
} // namespace

void ki::resolve::bind_mutability(Scope& scope, Mutability_bind const binding)
{
    do_bind(scope.mutabilities, binding);
}

void ki::resolve::bind_variable(Scope& scope, Variable_bind const binding)
{
    do_bind(scope.variables, binding);
}

void ki::resolve::bind_type(Scope& scope, Type_bind const binding)
{
    do_bind(scope.types, binding);
}

auto ki::resolve::find_mutability(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id)
    -> Mutability_bind*
{
    return do_find<Mutability_bind, &Scope::mutabilities>(ctx, scope_id, string_id);
}

auto ki::resolve::find_variable(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id)
    -> Variable_bind*
{
    return do_find<Variable_bind, &Scope::variables>(ctx, scope_id, string_id);
}

auto ki::resolve::find_type(Context& ctx, hir::Scope_id scope_id, utl::String_id string_id)
    -> Type_bind*
{
    return do_find<Type_bind, &Scope::types>(ctx, scope_id, string_id);
}

void ki::resolve::report_unused(ki::Database& db, Scope& scope)
{
    do_report_unused_bindings(db, scope.doc_id, scope.types);
    do_report_unused_bindings(db, scope.doc_id, scope.variables);
    do_report_unused_bindings(db, scope.doc_id, scope.mutabilities);
}

auto ki::resolve::make_scope(ki::Document_id doc_id) -> Scope
{
    return Scope {
        .variables    = {},
        .types        = {},
        .mutabilities = {},
        .doc_id       = doc_id,
        .parent_id    = std::nullopt,
    };
}
