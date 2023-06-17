#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;


namespace {
    struct Expression_desugaring_visitor {
        Desugar_context      & context;
        cst::Expression const& this_expression;

        auto operator()(cst::expression::Parenthesized const& parenthesized) -> hir::Expression::Variant {
            return utl::match(parenthesized.expression.value->value, Expression_desugaring_visitor { context, *parenthesized.expression.value });
        }
        template <class T>
        auto operator()(cst::expression::Literal<T> const& literal) -> hir::Expression::Variant {
            return hir::expression::Literal<T> { literal.value };
        }
        auto operator()(cst::expression::Array_literal const& literal) -> hir::Expression::Variant {
            return hir::expression::Array_literal {
                .elements = utl::map(context.deref_desugar(), literal.elements.value.elements),
            };
        }
        auto operator()(cst::expression::Self const&) -> hir::Expression::Variant {
            return hir::expression::Self {};
        }
        auto operator()(cst::expression::Variable const& variable) -> hir::Expression::Variant {
            return hir::expression::Variable {
                .name = context.desugar(variable.name),
            };
        }
        auto operator()(cst::expression::Tuple const& tuple) -> hir::Expression::Variant {
            return hir::expression::Tuple {
                .fields = utl::map(context.deref_desugar(), tuple.fields.value.elements),
            };
        }
        auto operator()(cst::expression::Conditional const&) -> hir::Expression::Variant {
            utl::todo();
        }
        auto operator()(cst::expression::Match const& match) -> hir::Expression::Variant {
            auto const desugar_match_case = [this](cst::expression::Match::Case const& match_case) {
                return hir::expression::Match::Case {
                    .pattern = context.desugar(match_case.pattern),
                    .handler = context.desugar(match_case.handler),
                };
            };
            return hir::expression::Match {
                .cases              = utl::map(desugar_match_case, match.cases),
                .matched_expression = context.desugar(match.matched_expression),
            };
        }
        auto operator()(cst::expression::Block const&) -> hir::Expression::Variant {
            utl::todo();
        }
        auto operator()(cst::expression::While_loop const& loop) -> hir::Expression::Variant {
            if (auto const* const let = std::get_if<cst::expression::Conditional_let>(&loop.condition->value)) {
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
                                    .handler = context.desugar(loop.body),
                                },
                                {
                                    .pattern = context.wildcard_pattern(this_expression.source_view),
                                    .handler = context.wrap(hir::Expression {
                                        .value = hir::expression::Break {
                                            .result = context.unit_value(this_expression.source_view),
                                        },
                                        .source_view = this_expression.source_view,
                                    })
                                }
                            }),
                            .matched_expression = context.desugar(let->initializer),
                        },
                        .source_view = loop.body->source_view,
                    }),
                    .source = hir::expression::Loop::Source::while_loop,
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
                                .result = context.unit_value(this_expression.source_view),
                            },
                            .source_view = this_expression.source_view,
                        }),
                        .kind                      = hir::expression::Conditional::Source::while_loop_body,
                        .has_explicit_false_branch = true,
                    },
                    .source_view = loop.body->source_view,
                }),
                .source = hir::expression::Loop::Source::while_loop,
            };
        }
        auto operator()(cst::expression::Infinite_loop const& loop) -> hir::Expression::Variant {
            return hir::expression::Loop {
                .body   = context.desugar(loop.body),
                .source = hir::expression::Loop::Source::plain_loop,
            };
        }
        auto operator()(cst::expression::Invocation const& invocation) -> hir::Expression::Variant {
            return hir::expression::Invocation {
                .arguments = utl::map(context.desugar(), invocation.function_arguments.value.elements),
                .invocable = context.desugar(invocation.function_expression),
            };
        }
        auto operator()(cst::expression::Struct_initializer const&) -> hir::Expression::Variant {
            utl::todo();
        }
        auto operator()(cst::expression::Binary_operator_invocation_sequence const&) -> hir::Expression::Variant {
            utl::todo();
        }
        auto operator()(cst::expression::Template_application const& application) -> hir::Expression::Variant {
            return hir::expression::Template_application {
                .template_arguments = utl::map(context.desugar(), application.template_arguments.value.elements),
                .name               = context.desugar(application.name),
            };
        }
        auto operator()(cst::expression::Struct_field_access const& access) -> hir::Expression::Variant {
            return hir::expression::Struct_field_access {
                .base_expression = context.desugar(access.base_expression),
                .field_name      = access.field_name,
            };
        }
        auto operator()(cst::expression::Tuple_field_access const& access) -> hir::Expression::Variant {
            return hir::expression::Tuple_field_access {
                .base_expression         = context.desugar(access.base_expression),
                .field_index             = access.field_index,
                .field_index_source_view = access.field_index_source_view,
            };
        }
        auto operator()(cst::expression::Array_index_access const& access) -> hir::Expression::Variant {
            return hir::expression::Array_index_access {
                .base_expression  = context.desugar(access.base_expression),
                .index_expression = context.desugar(access.index_expression),
            };
        }
        auto operator()(cst::expression::Method_invocation const&) -> hir::Expression::Variant {
            utl::todo();
        }
        auto operator()(cst::expression::Type_cast const& cast) -> hir::Expression::Variant {
            return hir::expression::Type_cast {
                .expression  = context.desugar(cast.base_expression),
                .target_type = context.desugar(cast.target_type),
            };
        }
        auto operator()(cst::expression::Type_ascription const& cast) -> hir::Expression::Variant {
            return hir::expression::Type_ascription {
                .expression    = context.desugar(cast.base_expression),
                .ascribed_type = context.desugar(cast.ascribed_type),
            };
        }
        auto operator()(cst::expression::Let_binding const& let) -> hir::Expression::Variant {
            return hir::expression::Let_binding {
                .pattern     = context.desugar(let.pattern),
                .initializer = context.desugar(let.initializer),
                .type        = let.type.transform(utl::compose(context.wrap(), context.desugar())),
            };
        }
        auto operator()(cst::expression::Local_type_alias const& alias) -> hir::Expression::Variant {
            return hir::expression::Local_type_alias {
                .alias_name   = alias.alias_name,
                .aliased_type = context.desugar(alias.aliased_type),
            };
        }
        auto operator()(cst::expression::Ret const& ret) -> hir::Expression::Variant {
            return hir::expression::Ret {
                .returned_expression = ret.returned_expression.transform(context.desugar()),
            };
        }
        auto operator()(cst::expression::Discard const& discard) -> hir::Expression::Variant {
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
                            .type        = tl::nullopt,
                        },
                        .source_view = this_expression.source_view,
                    }
                }),
                .result_expression = context.unit_value(this_expression.source_view),
            };
        }
        auto operator()(cst::expression::Break const& break_) -> hir::Expression::Variant {
            return hir::expression::Break {
                .result = break_.result.has_value()
                    ? context.desugar(*break_.result)
                    : context.unit_value(this_expression.source_view),
            };
        }
        auto operator()(cst::expression::Continue const&) -> hir::Expression::Variant {
            return hir::expression::Continue {};
        }
        auto operator()(cst::expression::Sizeof const& sizeof_) -> hir::Expression::Variant {
            return hir::expression::Sizeof { .inspected_type = context.desugar(sizeof_.inspected_type.value) };
        }
        auto operator()(cst::expression::Reference const& reference) -> hir::Expression::Variant {
            return hir::expression::Reference {
                .mutability            = context.desugar_mutability(reference.mutability, reference.ampersand_token.source_view),
                .referenced_expression = context.desugar(reference.referenced_expression),
            };
        }
        auto operator()(cst::expression::Addressof const& addressof) -> hir::Expression::Variant {
            return hir::expression::Addressof {
                .lvalue_expression = context.desugar(addressof.lvalue_expression.value),
            };
        }
        auto operator()(cst::expression::Reference_dereference const& dereference) -> hir::Expression::Variant {
            return hir::expression::Reference_dereference {
                .dereferenced_expression = context.desugar(dereference.dereferenced_expression),
            };
        }
        auto operator()(cst::expression::Pointer_dereference const& dereference) -> hir::Expression::Variant {
            return hir::expression::Pointer_dereference {
                .pointer_expression = context.desugar(dereference.pointer_expression.value),
            };
        }
        auto operator()(cst::expression::Unsafe const& unsafe) -> hir::Expression::Variant {
            return hir::expression::Unsafe { .expression = context.desugar(unsafe.expression) };
        }
        auto operator()(cst::expression::Move const& move) -> hir::Expression::Variant {
            return hir::expression::Move { .lvalue = context.desugar(move.lvalue) };
        }
        auto operator()(cst::expression::Meta const& meta) -> hir::Expression::Variant {
            return hir::expression::Meta { .expression = context.desugar(meta.expression.value) };
        }
        auto operator()(cst::expression::Hole const&) -> hir::Expression::Variant {
            return hir::expression::Hole {};
        }

        auto operator()(cst::expression::For_loop const&) -> hir::Expression::Variant {
            context.error(this_expression.source_view, { "For loops are not supported yet" });
        }

        auto operator()(cst::expression::Conditional_let const&) -> hir::Expression::Variant {
            // Should be unreachable because a conditional let expression can
            // only occur as the condition of if-let or while-let expressions.
            utl::unreachable();
        }

    };
}


auto libdesugar::Desugar_context::desugar(cst::Expression const& expression) -> hir::Expression {
    return {
        .value = utl::match(expression.value, Expression_desugaring_visitor { *this, expression }),
        .source_view = expression.source_view,
    };
}
