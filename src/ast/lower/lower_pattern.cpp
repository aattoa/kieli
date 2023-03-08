#include "utl/utilities.hpp"
#include "lowering_internals.hpp"


namespace {

    struct Pattern_lowering_visitor {
        Lowering_context& context;

        template <class T>
        auto operator()(ast::pattern::Literal<T> const& literal) -> hir::Pattern::Variant {
            return literal;
        }

        auto operator()(ast::pattern::Wildcard const&) -> hir::Pattern::Variant {
            return hir::pattern::Wildcard {};
        }

        auto operator()(ast::pattern::Name const& name) -> hir::Pattern::Variant {
            return hir::pattern::Name {
                .identifier = name.identifier,
                .mutability = name.mutability
            };
        }

        auto operator()(ast::pattern::Tuple const& tuple) -> hir::Pattern::Variant {
            return hir::pattern::Tuple {
                .field_patterns = utl::map(context.lower())(tuple.field_patterns)
            };
        }

        auto operator()(ast::pattern::Slice const& slice) -> hir::Pattern::Variant {
            return hir::pattern::Slice {
                .element_patterns = utl::map(context.lower())(slice.element_patterns)
            };
        }

        auto operator()(ast::pattern::Constructor const& ctor) -> hir::Pattern::Variant {
            return hir::pattern::Constructor {
                .constructor_name = context.lower(ctor.constructor_name),
                .payload_pattern  = ctor.payload_pattern.transform(context.lower())
            };
        }

        auto operator()(ast::pattern::As const& as) -> hir::Pattern::Variant {
            return hir::pattern::As {
                .alias           = as.alias,
                .aliased_pattern = context.lower(as.aliased_pattern)
            };
        }

        auto operator()(ast::pattern::Guarded const& guarded) -> hir::Pattern::Variant {
            return hir::pattern::Guarded {
                .guarded_pattern = context.lower(guarded.guarded_pattern),
                .guard           = context.lower(guarded.guard)
            };
        }
    };

}


auto Lowering_context::lower(ast::Pattern const& pattern) -> hir::Pattern {
    return {
        .value = std::visit(Pattern_lowering_visitor { .context = *this }, pattern.value),
        .source_view = pattern.source_view
    };
}