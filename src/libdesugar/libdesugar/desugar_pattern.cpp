#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;


namespace {
    struct Pattern_desugaring_visitor {
        Desugar_context& context;

        auto operator()(cst::pattern::Parenthesized const& parenthesized) -> hir::Pattern::Variant {
            return std::visit(Pattern_desugaring_visitor { context }, parenthesized.pattern.value->value);
        }
        template <class T>
        auto operator()(cst::pattern::Literal<T> const& literal) -> hir::Pattern::Variant {
            return hir::pattern::Literal<T> { literal.value };
        }
        auto operator()(cst::pattern::Wildcard const&) -> hir::Pattern::Variant {
            return hir::pattern::Wildcard {};
        }
        auto operator()(cst::pattern::Name const& name) -> hir::Pattern::Variant {
            return hir::pattern::Name {
                .name       = name.name,
                .mutability = context.desugar_mutability(name.mutability, name.name.source_view),
            };
        }
        auto operator()(cst::pattern::Tuple const& tuple) -> hir::Pattern::Variant {
            return hir::pattern::Tuple {
                .field_patterns = utl::map(context.deref_desugar(), tuple.patterns.value.elements),
            };
        }
        auto operator()(cst::pattern::Top_level_tuple const& tuple) -> hir::Pattern::Variant {
            return hir::pattern::Tuple {
                .field_patterns = utl::map(context.deref_desugar(), tuple.patterns.elements),
            };
        }
        auto operator()(cst::pattern::Slice const& slice) -> hir::Pattern::Variant {
            return hir::pattern::Slice {
                .element_patterns = utl::map(context.deref_desugar(), slice.patterns.value.elements),
            };
        }
        auto operator()(cst::pattern::Constructor const& ctor) -> hir::Pattern::Variant {
            return hir::pattern::Constructor {
                .constructor_name = context.desugar(ctor.constructor_name),
                .payload_pattern  = ctor.payload_pattern.transform(utl::compose(context.desugar(), cst::surrounded_value)),
            };
        }
        auto operator()(cst::pattern::Abbreviated_constructor const& ctor) -> hir::Pattern::Variant {
            return hir::pattern::Abbreviated_constructor {
                .constructor_name = ctor.constructor_name,
                .payload_pattern  = ctor.payload_pattern.transform(utl::compose(context.desugar(), cst::surrounded_value)),
            };
        }
        auto operator()(cst::pattern::Alias const& alias) -> hir::Pattern::Variant {
            return hir::pattern::Alias {
                .alias_name       = alias.alias_name,
                .alias_mutability = context.desugar_mutability(alias.alias_mutability, alias.alias_name.source_view),
                .aliased_pattern  = context.desugar(alias.aliased_pattern),
            };
        }
        auto operator()(cst::pattern::Guarded const& guarded) -> hir::Pattern::Variant {
            return hir::pattern::Guarded {
                .guarded_pattern = context.desugar(guarded.guarded_pattern),
                .guard           = context.desugar(*guarded.guard_expression),
            };
        }
    };
}


auto libdesugar::Desugar_context::desugar(cst::Pattern const& pattern) -> hir::Pattern {
    return {
        .value = std::visit(Pattern_desugaring_visitor { *this }, pattern.value),
        .source_view = pattern.source_view,
    };
}
