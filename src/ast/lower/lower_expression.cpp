#include "utl/utilities.hpp"
#include "lowering_internals.hpp"


namespace {

    struct Expression_lowering_visitor {
        Lowering_context     & context;
        ast::Expression const& this_expression;


        template <class T>
        auto operator()(ast::expression::Literal<T> const& literal) -> hir::Expression::Variant {
            return literal;
        }
        auto operator()(ast::expression::Array_literal const& literal) -> hir::Expression::Variant {
            return hir::expression::Array_literal {
                .elements = utl::map(context.lower(), literal.elements)
            };
        }
        auto operator()(ast::expression::Self const& self) -> hir::Expression::Variant {
            return self;
        }
        auto operator()(ast::expression::Variable const& variable) -> hir::Expression::Variant {
            return hir::expression::Variable {
                .name = context.lower(variable.name)
            };
        }
        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression::Variant {
            return hir::expression::Tuple {
                .fields = utl::map(context.lower(), tuple.fields)
            };
        }
        auto operator()(ast::expression::Conditional const& conditional) -> hir::Expression::Variant {
            utl::Wrapper<hir::Expression> false_branch
                = conditional.false_branch
                . transform(context.lower())
                . value_or(context.unit_value(this_expression.source_view));

            if (auto* const let = std::get_if<ast::expression::Conditional_let>(&conditional.condition->value)) {
                /*
                    if let a = b { c } else { d }

                    is transformed into

                    match b {
                        a -> c
                        _ -> d
                    }
                */

                return hir::expression::Match {
                    .cases = utl::to_vector<hir::expression::Match::Case>({
                        {
                            .pattern = context.lower(let->pattern),
                            .handler = context.lower(conditional.true_branch)
                        },
                        {
                            .pattern = utl::wrap(hir::Pattern {
                                .value       = hir::pattern::Wildcard {},
                                .source_view = let->pattern->source_view
                            }),
                            .handler = std::move(false_branch)
                        }
                    }),
                    .matched_expression = context.lower(let->initializer)
                };
            }
            else {
                return hir::expression::Conditional {
                    .condition    = context.lower(conditional.condition),
                    .true_branch  = context.lower(conditional.true_branch),
                    .false_branch = false_branch
                };
            }
        }
        auto operator()(ast::expression::Match const& match) -> hir::Expression::Variant {
            auto const lower_match_case = [this](ast::expression::Match::Case const& match_case)
                -> hir::expression::Match::Case
            {
                return {
                    .pattern = context.lower(match_case.pattern),
                    .handler = context.lower(match_case.handler)
                };
            };

            return hir::expression::Match {
                .cases              = utl::map(lower_match_case, match.cases),
                .matched_expression = context.lower(match.matched_expression)
            };
        }
        auto operator()(ast::expression::Block const& block) -> hir::Expression::Variant {
            return hir::expression::Block {
                .side_effects = utl::map(context.lower(), block.side_effects),
                .result       = block.result.transform(context.lower())
            };
        }
        auto operator()(ast::expression::While_loop const& loop) -> hir::Expression::Variant {
            if (auto* const let = std::get_if<ast::expression::Conditional_let>(&loop.condition->value)) {
                /*
                    while let a = b { c }

                    is transformed into

                    loop {
                        match b {
                            a -> c
                            _ -> break
                        }
                    }
                */

                return hir::expression::Loop {
                    .body = utl::wrap(hir::Expression {
                        .value = hir::expression::Match {
                            .cases = utl::to_vector<hir::expression::Match::Case>({
                                {
                                    .pattern = context.lower(let->pattern),
                                    .handler = context.lower(loop.body)
                                },
                                {
                                    .pattern = context.wildcard_pattern(this_expression.source_view),
                                    .handler = utl::wrap(hir::Expression {
                                        .value       = hir::expression::Break {},
                                        .source_view = this_expression.source_view
                                    })
                                }
                            }),
                            .matched_expression = context.lower(let->initializer)
                        },
                        .source_view = loop.body->source_view
                    })
                };
            }

            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */

            return hir::expression::Loop {
                .body = utl::wrap(hir::Expression {
                    .value = hir::expression::Conditional {
                        .condition = context.lower(loop.condition),
                        .true_branch = context.lower(loop.body),
                        .false_branch = utl::wrap(hir::Expression {
                            .value       = hir::expression::Break {},
                            .source_view = this_expression.source_view
                        })
                    },
                    .source_view = loop.body->source_view
                })
            };
        }
        auto operator()(ast::expression::Infinite_loop const& loop) -> hir::Expression::Variant {
            return hir::expression::Loop { .body = context.lower(loop.body) };
        }
        auto operator()(ast::expression::Invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Invocation {
                .arguments = utl::map(context.lower(), invocation.arguments),
                .invocable = context.lower(invocation.invocable)
            };
        }
        auto operator()(ast::expression::Struct_initializer const& initializer) -> hir::Expression::Variant {
            decltype(hir::expression::Struct_initializer::member_initializers) initializers;
            initializers.container().reserve(initializer.member_initializers.size());

            for (auto& [name, init] : initializer.member_initializers.span()) {
                initializers.add(utl::copy(name), context.lower(init));
            }

            return hir::expression::Struct_initializer {
                .member_initializers = std::move(initializers),
                .struct_type         = context.lower(initializer.struct_type)
            };
        }
        auto operator()(ast::expression::Binary_operator_invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Binary_operator_invocation {
                .left  = context.lower(invocation.left),
                .right = context.lower(invocation.right),
                .op    = invocation.op
            };
        }
        auto operator()(ast::expression::Template_application const& application) -> hir::Expression::Variant {
            return hir::expression::Template_application {
                .template_arguments = utl::map(context.lower(), application.template_arguments),
                .name               = context.lower(application.name)
            };
        }
        auto operator()(ast::expression::Member_access_chain const& chain) -> hir::Expression::Variant {
            using AST_chain = ast::expression::Member_access_chain;
            using HIR_chain = hir::expression::Member_access_chain;

            auto const lower_accessor = [this](AST_chain::Accessor const& accessor) {
                return HIR_chain::Accessor {
                    .value = std::visit<HIR_chain::Accessor::Variant>(utl::Overload {
                        [](AST_chain::Tuple_field const& field) {
                            return HIR_chain::Tuple_field {.index = field.index };
                        },
                        [](AST_chain::Struct_field const& field) {
                            return HIR_chain::Struct_field {.identifier = field.identifier };
                        },
                        [this](AST_chain::Array_index const& index) {
                            return HIR_chain::Array_index {.expression = context.lower(index.expression) };
                        }
                    }, accessor.value),
                    .source_view = accessor.source_view
                };
            };

            return hir::expression::Member_access_chain {
                .accessors       = utl::map(lower_accessor, chain.accessors),
                .base_expression = context.lower(chain.base_expression)
            };
        }
        auto operator()(ast::expression::Method_invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Method_invocation {
                .arguments          = utl::map(context.lower(), invocation.arguments),
                .template_arguments = invocation.template_arguments.transform(utl::map(context.lower())),
                .base_expression    = context.lower(invocation.base_expression),
                .method_name        = invocation.method_name
            };
        }
        auto operator()(ast::expression::Type_cast const& cast) -> hir::Expression::Variant {
            return hir::expression::Type_cast {
                .expression  = context.lower(cast.expression),
                .target_type = context.lower(cast.target_type),
                .cast_kind   = cast.cast_kind
            };
        }
        auto operator()(ast::expression::Let_binding const& let) -> hir::Expression::Variant {
            return hir::expression::Let_binding {
                .pattern     = context.lower(let.pattern),
                .initializer = context.lower(let.initializer),
                .type        = let.type.transform(context.lower())
            };
        }
        auto operator()(ast::expression::Local_type_alias const& alias) -> hir::Expression::Variant {
            return hir::expression::Local_type_alias {
                .identifier   = alias.identifier,
                .aliased_type = context.lower(alias.aliased_type)
            };
        }
        auto operator()(ast::expression::Ret const& ret) -> hir::Expression::Variant {
            return hir::expression::Ret {
                .returned_expression = ret.returned_expression.transform(context.lower())
            };
        }
        auto operator()(ast::expression::Discard const& discard) -> hir::Expression::Variant {
            /*
                discard x

                is transformed into

                { let _ = x; }
            */

            return hir::expression::Block {
                .side_effects = utl::to_vector<hir::Expression>({
                    {
                        .value = hir::expression::Let_binding {
                            .pattern     = context.wildcard_pattern(this_expression.source_view),
                            .initializer = context.lower(discard.discarded_expression),
                            .type        = tl::nullopt
                        },
                        .source_view = this_expression.source_view
                    }
                })
            };
        }
        auto operator()(ast::expression::Break const& break_) -> hir::Expression::Variant {
            return hir::expression::Break {
                .label  = break_.label,
                .result = break_.result.transform(context.lower())
            };
        }
        auto operator()(ast::expression::Continue const&) -> hir::Expression::Variant {
            return hir::expression::Continue {};
        }
        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression::Variant {
            return hir::expression::Sizeof { .inspected_type = context.lower(sizeof_.inspected_type) };
        }
        auto operator()(ast::expression::Reference const& reference) -> hir::Expression::Variant {
            return hir::expression::Reference {
                .mutability            = reference.mutability,
                .referenced_expression = context.lower(reference.referenced_expression)
            };
        }
        auto operator()(ast::expression::Dereference const& dereference) -> hir::Expression::Variant {
            return hir::expression::Dereference {
                .dereferenced_expression = context.lower(dereference.dereferenced_expression)
            };
        }
        auto operator()(ast::expression::Addressof const& addressof) -> hir::Expression::Variant {
            return hir::expression::Addressof {
                .lvalue = context.lower(addressof.lvalue)
            };
        }
        auto operator()(ast::expression::Unsafe_dereference const& dereference) -> hir::Expression::Variant {
            return hir::expression::Unsafe_dereference {
                .pointer = context.lower(dereference.pointer)
            };
        }

        auto operator()(ast::expression::Placement_init const& init) -> hir::Expression::Variant {
            return hir::expression::Placement_init {
                .lvalue      = context.lower(init.lvalue),
                .initializer = context.lower(init.initializer)
            };
        }
        auto operator()(ast::expression::Move const& move) -> hir::Expression::Variant {
            return hir::expression::Move { context.lower(move.lvalue) };
        }
        auto operator()(ast::expression::Meta const& meta) -> hir::Expression::Variant {
            return hir::expression::Meta { .expression = context.lower(meta.expression) };
        }
        auto operator()(ast::expression::Hole const&) -> hir::Expression::Variant {
            return hir::expression::Hole {};
        }

        auto operator()(ast::expression::For_loop const&) -> hir::Expression::Variant {
            context.error(this_expression.source_view, { "For loops are not supported yet" });
        }
        auto operator()(ast::expression::Lambda const&) -> hir::Expression::Variant {
            context.error(this_expression.source_view, { "Lambda expressions are not supported yet" });
        }

        auto operator()(ast::expression::Conditional_let const&) -> hir::Expression::Variant {
            // Should be unreachable because a conditional let expression can
            // only occur as the condition of if-let or while-let expressions.
            utl::abort();
        }

    };

}


auto Lowering_context::lower(ast::Expression const& expression) -> hir::Expression {
    return {
        .value = std::visit(Expression_lowering_visitor { *this, expression }, expression.value),
        .source_view = expression.source_view
    };
}