#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>


namespace {
    struct Pattern_desugaring_visitor {
        Desugaring_context& context;

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
                .field_patterns = utl::map(context.desugar(), tuple.field_patterns),
            };
        }
        auto operator()(ast::pattern::Slice const& slice) -> hir::Pattern::Variant {
            return hir::pattern::Slice {
                .element_patterns = utl::map(context.desugar(), slice.element_patterns),
            };
        }
        auto operator()(ast::pattern::Constructor const& ctor) -> hir::Pattern::Variant {
            return hir::pattern::Constructor {
                .constructor_name = context.desugar(ctor.constructor_name),
                .payload_pattern  = ctor.payload_pattern.transform(context.desugar()),
            };
        }
        auto operator()(ast::pattern::Abbreviated_constructor const& ctor) -> hir::Pattern::Variant {
            return hir::pattern::Abbreviated_constructor {
                .constructor_name = ctor.constructor_name,
                .payload_pattern  = ctor.payload_pattern.transform(context.desugar()),
            };
        }
        auto operator()(ast::pattern::As const& as) -> hir::Pattern::Variant {
            return hir::pattern::As {
                .alias           = as.alias,
                .aliased_pattern = context.desugar(as.aliased_pattern),
            };
        }
        auto operator()(ast::pattern::Guarded const& guarded) -> hir::Pattern::Variant {
            return hir::pattern::Guarded {
                .guarded_pattern = context.desugar(guarded.guarded_pattern),
                .guard           = context.desugar(guarded.guard),
            };
        }
    };
}


auto Desugaring_context::desugar(ast::Pattern const& pattern) -> hir::Pattern {
    return {
        .value = std::visit(Pattern_desugaring_visitor { *this }, pattern.value),
        .source_view = pattern.source_view
    };
}
