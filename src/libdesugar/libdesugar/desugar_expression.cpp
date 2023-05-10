#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>


namespace {

    struct Expression_desugaring_visitor {
        Desugaring_context   & context;
        ast::Expression const& this_expression;


        template <class T>
        auto operator()(ast::expression::Literal<T> const& literal) -> hir::Expression::Variant {
            return hir::expression::Literal<T> { literal.value };
        }
        auto operator()(ast::expression::Array_literal const& literal) -> hir::Expression::Variant {
            return hir::expression::Array_literal {
                .elements = utl::map(context.desugar(), literal.elements)
            };
        }
        auto operator()(ast::expression::Self const&) -> hir::Expression::Variant {
            return hir::expression::Self {};
        }
        auto operator()(ast::expression::Variable const& variable) -> hir::Expression::Variant {
            return hir::expression::Variable {
                .name = context.desugar(variable.name)
            };
        }
        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression::Variant {
            return hir::expression::Tuple {
                .fields = utl::map(context.desugar(), tuple.fields)
            };
        }
        auto operator()(ast::expression::Conditional const& conditional) -> hir::Expression::Variant {
            utl::Wrapper<hir::Expression> const false_branch
                = conditional.false_branch.has_value()
                ? context.desugar(*conditional.false_branch)
                : context.unit_value(this_expression.source_view);

            if (auto const* const let = std::get_if<ast::expression::Conditional_let>(&conditional.condition->value)) {
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
                            .pattern = context.desugar(let->pattern),
                            .handler = context.desugar(conditional.true_branch)
                        },
                        {
                            .pattern = context.wrap(hir::Pattern {
                                .value       = hir::pattern::Wildcard {},
                                .source_view = let->pattern->source_view
                            }),
                            .handler = false_branch
                        }
                    }),
                    .matched_expression = context.desugar(let->initializer)
                };
            }
            else {
                return hir::expression::Conditional {
                    .condition                 = context.desugar(conditional.condition),
                    .true_branch               = context.desugar(conditional.true_branch),
                    .false_branch              = false_branch,
                    .kind                      = hir::expression::Conditional::Kind::normal_conditional,
                    .has_explicit_false_branch = conditional.false_branch.has_value()
                };
            }
        }
        auto operator()(ast::expression::Match const& match) -> hir::Expression::Variant {
            auto const desugar_match_case = [this](ast::expression::Match::Case const& match_case) {
                return hir::expression::Match::Case {
                    .pattern = context.desugar(match_case.pattern),
                    .handler = context.desugar(match_case.handler)
                };
            };
            return hir::expression::Match {
                .cases              = utl::map(desugar_match_case, match.cases),
                .matched_expression = context.desugar(match.matched_expression)
            };
        }
        auto operator()(ast::expression::Block const& block) -> hir::Expression::Variant {
            return hir::expression::Block {
                .side_effect_expressions = utl::map(context.desugar(), block.side_effect_expressions),
                .result_expression = block.result_expression.has_value()
                    ? context.desugar(*block.result_expression)
                    : context.unit_value(this_expression.source_view)
            };
        }
        auto operator()(ast::expression::While_loop const& loop) -> hir::Expression::Variant {
            if (auto const* const let = std::get_if<ast::expression::Conditional_let>(&loop.condition->value)) {
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
                    .body = context.wrap(hir::Expression {
                        .value = hir::expression::Match {
                            .cases = utl::to_vector<hir::expression::Match::Case>({
                                {
                                    .pattern = context.desugar(let->pattern),
                                    .handler = context.desugar(loop.body)
                                },
                                {
                                    .pattern = context.wildcard_pattern(this_expression.source_view),
                                    .handler = context.wrap(hir::Expression {
                                        .value = hir::expression::Break {
                                            .result = context.unit_value(this_expression.source_view)
                                        },
                                        .source_view = this_expression.source_view
                                    })
                                }
                            }),
                            .matched_expression = context.desugar(let->initializer)
                        },
                        .source_view = loop.body->source_view
                    }),
                    .kind = hir::expression::Loop::Kind::while_loop
                };
            }

            /*
                while a { b }

                is transformed into

                loop { if a { b } else { break } }
            */

            return hir::expression::Loop {
                .body = context.wrap(hir::Expression {
                    .value = hir::expression::Conditional {
                        .condition = context.desugar(loop.condition),
                        .true_branch = context.desugar(loop.body),
                        .false_branch = context.wrap(hir::Expression {
                            .value = hir::expression::Break {
                                .result = context.unit_value(this_expression.source_view)
                            },
                            .source_view = this_expression.source_view
                        }),
                        .kind                      = hir::expression::Conditional::Kind::while_loop_body,
                        .has_explicit_false_branch = true
                    },
                    .source_view = loop.body->source_view
                }),
                .kind = hir::expression::Loop::Kind::while_loop
            };
        }
        auto operator()(ast::expression::Infinite_loop const& loop) -> hir::Expression::Variant {
            return hir::expression::Loop {
                .body = context.desugar(loop.body),
                .kind = hir::expression::Loop::Kind::plain_loop
            };
        }
        auto operator()(ast::expression::Invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Invocation {
                .arguments = utl::map(context.desugar(), invocation.arguments),
                .invocable = context.desugar(invocation.invocable)
            };
        }
        auto operator()(ast::expression::Struct_initializer const& initializer) -> hir::Expression::Variant {
            decltype(hir::expression::Struct_initializer::member_initializers) initializers
                { utl::vector_with_capacity(initializer.member_initializers.size()) };

            for (auto const& [name, init] : initializer.member_initializers.span()) // NOLINT
                initializers.add_or_assign(name, context.desugar(init));

            return hir::expression::Struct_initializer {
                .member_initializers = std::move(initializers),
                .struct_type         = context.desugar(initializer.struct_type)
            };
        }
        auto operator()(ast::expression::Binary_operator_invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Binary_operator_invocation {
                .left  = context.desugar(invocation.left),
                .right = context.desugar(invocation.right),
                .op    = invocation.op
            };
        }
        auto operator()(ast::expression::Template_application const& application) -> hir::Expression::Variant {
            return hir::expression::Template_application {
                .template_arguments = utl::map(context.desugar(), application.template_arguments),
                .name               = context.desugar(application.name)
            };
        }
        auto operator()(ast::expression::Struct_field_access const& access) -> hir::Expression::Variant {
            return hir::expression::Struct_field_access {
                .base_expression = context.desugar(access.base_expression),
                .field_name      = access.field_name
            };
        }
        auto operator()(ast::expression::Tuple_field_access const& access) -> hir::Expression::Variant {
            return hir::expression::Tuple_field_access {
                .base_expression         = context.desugar(access.base_expression),
                .field_index             = access.field_index,
                .field_index_source_view = access.field_index_source_view
            };
        }
        auto operator()(ast::expression::Array_index_access const& access) -> hir::Expression::Variant {
            return hir::expression::Array_index_access {
                .base_expression  = context.desugar(access.base_expression),
                .index_expression = context.desugar(access.index_expression),
            };
        }
        auto operator()(ast::expression::Method_invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Method_invocation {
                .arguments          = utl::map(context.desugar(), invocation.arguments),
                .template_arguments = invocation.template_arguments.transform(utl::map(context.desugar())),
                .base_expression    = context.desugar(invocation.base_expression),
                .method_name        = invocation.method_name
            };
        }
        auto operator()(ast::expression::Type_cast const& cast) -> hir::Expression::Variant {
            return hir::expression::Type_cast {
                .expression  = context.desugar(cast.expression),
                .target_type = context.desugar(cast.target_type),
                .cast_kind   = cast.cast_kind
            };
        }
        auto operator()(ast::expression::Let_binding const& let) -> hir::Expression::Variant {
            return hir::expression::Let_binding {
                .pattern     = context.desugar(let.pattern),
                .initializer = context.desugar(let.initializer),
                .type        = let.type.transform(context.desugar())
            };
        }
        auto operator()(ast::expression::Local_type_alias const& alias) -> hir::Expression::Variant {
            return hir::expression::Local_type_alias {
                .identifier   = alias.identifier,
                .aliased_type = context.desugar(alias.aliased_type)
            };
        }
        auto operator()(ast::expression::Ret const& ret) -> hir::Expression::Variant {
            return hir::expression::Ret {
                .returned_expression = ret.returned_expression.transform(context.desugar())
            };
        }
        auto operator()(ast::expression::Discard const& discard) -> hir::Expression::Variant {
            /*
                discard x

                is transformed into

                { let _ = x; () }
            */

            return hir::expression::Block {
                .side_effect_expressions = utl::to_vector({
                    hir::Expression {
                        .value = hir::expression::Let_binding {
                            .pattern     = context.wildcard_pattern(this_expression.source_view),
                            .initializer = context.desugar(discard.discarded_expression),
                            .type        = tl::nullopt
                        },
                        .source_view = this_expression.source_view
                    }
                }),
                .result_expression = context.unit_value(this_expression.source_view)
            };
        }
        auto operator()(ast::expression::Break const& break_) -> hir::Expression::Variant {
            return hir::expression::Break {
                .label  = break_.label,
                .result = break_.result.has_value()
                    ? context.desugar(*break_.result)
                    : context.unit_value(this_expression.source_view)
            };
        }
        auto operator()(ast::expression::Continue const&) -> hir::Expression::Variant {
            return hir::expression::Continue {};
        }
        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression::Variant {
            return hir::expression::Sizeof { .inspected_type = context.desugar(sizeof_.inspected_type) };
        }
        auto operator()(ast::expression::Reference const& reference) -> hir::Expression::Variant {
            return hir::expression::Reference {
                .mutability            = reference.mutability,
                .referenced_expression = context.desugar(reference.referenced_expression)
            };
        }
        auto operator()(ast::expression::Dereference const& dereference) -> hir::Expression::Variant {
            return hir::expression::Dereference {
                .dereferenced_expression = context.desugar(dereference.dereferenced_expression)
            };
        }
        auto operator()(ast::expression::Addressof const& addressof) -> hir::Expression::Variant {
            return hir::expression::Addressof {
                .lvalue = context.desugar(addressof.lvalue)
            };
        }
        auto operator()(ast::expression::Unsafe_dereference const& dereference) -> hir::Expression::Variant {
            return hir::expression::Unsafe_dereference {
                .pointer = context.desugar(dereference.pointer)
            };
        }

        auto operator()(ast::expression::Placement_init const& init) -> hir::Expression::Variant {
            return hir::expression::Placement_init {
                .lvalue      = context.desugar(init.lvalue),
                .initializer = context.desugar(init.initializer)
            };
        }
        auto operator()(ast::expression::Move const& move) -> hir::Expression::Variant {
            return hir::expression::Move { context.desugar(move.lvalue) };
        }
        auto operator()(ast::expression::Meta const& meta) -> hir::Expression::Variant {
            return hir::expression::Meta { .expression = context.desugar(meta.expression) };
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


auto Desugaring_context::desugar(ast::Expression const& expression) -> hir::Expression {
    return {
        .value = std::visit(Expression_desugaring_visitor { *this, expression }, expression.value),
        .source_view = expression.source_view
    };
}
