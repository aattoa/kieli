#include "utl/utilities.hpp"
#include "parser_internals.hpp"


namespace {

    template <class T>
    auto extract_literal(Parse_context& context)
        -> ast::Expression::Variant
    {
        return ast::expression::Literal<T> { context.previous().value_as<T>() };
    }


    auto parse_struct_member_initializer(Parse_context& context)
        -> tl::optional<utl::Pair<ast::Name, utl::Wrapper<ast::Expression>>>
    {
        if (auto member = parse_lower_name(context)) {
            context.consume_required(Token::Type::equals);
            return utl::Pair { *member, utl::wrap(extract_expression(context)) };
        }
        return tl::nullopt;
    }

    auto extract_struct_initializer(ast::Type&& type, Parse_context& context)
        -> ast::Expression::Variant
    {
        static constexpr auto extract_member_initializers =
            extract_comma_separated_zero_or_more<parse_struct_member_initializer, "a member initializer">;

        auto initializers = extract_member_initializers(context);
        context.consume_required(Token::Type::brace_close);

        for (auto it = initializers.begin(); it != initializers.end(); ++it) {
            std::vector<utl::Source_view> duplicates;

            for (auto& [name, initializer] : std::span { it + 1, initializers.end() }) {
                if (it->first == name)
                    duplicates.push_back(name.source_view);
            }

            if (!duplicates.empty()) {
                duplicates.insert(duplicates.begin(), it->first.source_view);

                auto const make_section = [&](utl::Source_view const source_view) {
                    return utl::diagnostics::Text_section {
                        .source_view = source_view,
                        .source      = context.source,
                        .note_color  = utl::diagnostics::error_color,
                    };
                };

                context.compilation_info.diagnostics.emit_error({
                    .sections = utl::map(make_section, duplicates),
                    .message  = "There are multiple initializers for the same field"
                });
            }
        }

        return ast::expression::Struct_initializer {
            .member_initializers = utl::Flatmap { std::move(initializers) },
            .struct_type         = utl::wrap(std::move(type))
        };
    }


    auto extract_qualified_lower_name_or_struct_initializer(ast::Root_qualifier&& root, Parse_context& context)
        -> ast::Expression::Variant
    {
        auto* const anchor = context.pointer;
        auto        name   = extract_qualified(std::move(root), context);

        auto template_arguments = parse_template_arguments(context);

        if (!name.primary_name.is_upper) {
            if (template_arguments) {
                return ast::expression::Template_application {
                    std::move(*template_arguments),
                    std::move(name)
                };
            }
            return ast::expression::Variable { std::move(name) };
        }
        else if (context.try_consume(Token::Type::brace_open)) {
            auto value = std::invoke([&]() -> ast::Type::Variant {
                if (template_arguments) {
                    return ast::type::Template_application {
                        std::move(*template_arguments),
                        std::move(name)
                    };
                }
                return ast::type::Typename { std::move(name) };
            });

            return extract_struct_initializer(
                ast::Type {
                    .value       = std::move(value),
                    .source_view = make_source_view(anchor, context.pointer - 1)
                },
                context);
        }
        context.error(
            { anchor, context.pointer },
            "Expected an expression, but found a type");
    }


    auto extract_condition(Parse_context& context)
        -> ast::Expression
    {
        Token const* const anchor = context.pointer;

        if (context.try_consume(Token::Type::let)) {
            auto pattern = extract_pattern(context);
            context.consume_required(Token::Type::equals);

            return {
                .value = ast::expression::Conditional_let {
                    .pattern     = utl::wrap(std::move(pattern)),
                    .initializer = utl::wrap(extract_expression(context))
                },
                .source_view = make_source_view(anchor, context.pointer - 1)
            };
        }
        return extract_expression(context);
    }


    auto extract_loop_body(Parse_context& context) -> ast::Expression {
        if (auto body = parse_block_expression(context))
            return std::move(*body);
        else
            context.error_expected("the loop body", "the loop body must be a block expression");
    }

    auto extract_any_loop(Parse_context& context, tl::optional<ast::Name> const label = tl::nullopt)
        -> ast::Expression::Variant
    {
        context.retreat();

        switch (context.extract().type) {
        case Token::Type::loop:
        {
            return ast::expression::Infinite_loop {
                .label = label,
                .body  = utl::wrap(extract_loop_body(context))
            };
        }
        case Token::Type::while_:
        {
            auto condition = extract_condition(context);

            if (auto const* const literal = std::get_if<ast::expression::Literal<compiler::Boolean>>(&condition.value)) {
                if (literal->value.value) {
                    context.compilation_info.diagnostics.emit_simple_note({
                        .erroneous_view = condition.source_view,
                        .source         = context.source,
                        .message        = "Consider using 'loop' instead of 'while true'",
                    });
                }
                else {
                    context.compilation_info.diagnostics.emit_simple_warning({
                        .erroneous_view = condition.source_view,
                        .source         = context.source,
                        .message        = "Loop will never be run"
                    });
                }
            }

            return ast::expression::While_loop {
                .label     = label,
                .condition = utl::wrap(std::move(condition)),
                .body      = utl::wrap(extract_loop_body(context))
            };
        }
        case Token::Type::for_:
        {
            auto iterator = extract_pattern(context);
            context.consume_required(Token::Type::in);
            auto iterable = extract_expression(context);

            return ast::expression::For_loop {
                .label    = label,
                .iterator = utl::wrap(std::move(iterator)),
                .iterable = utl::wrap(std::move(iterable)),
                .body     = utl::wrap(extract_loop_body(context))
            };
        }
        default:
            utl::unreachable();
        }
    }

    auto extract_loop(Parse_context& context) -> ast::Expression::Variant {
        return extract_any_loop(context);
    }


    auto extract_identifier(Parse_context& context)
        -> ast::Expression::Variant
    {
        switch (context.pointer->type) {
        case Token::Type::loop:
        case Token::Type::while_:
        case Token::Type::for_:
            if (Token const* const name_token = &context.previous(); name_token->type == Token::Type::lower_name) {
                ++context.pointer;
                return extract_any_loop(
                    context,
                    ast::Name {
                        .identifier  = name_token->as_identifier(),
                        .is_upper    = name_token->type == Token::Type::upper_name,
                        .source_view = name_token->source_view
                    });
            }
            else {
                context.error(name_token->source_view, { "Loop labels must be lowercase" });
            }
        default:
            context.retreat();
            return extract_qualified_lower_name_or_struct_initializer({ std::monostate {} }, context);
        }
    }

    auto extract_global_identifier(Parse_context& context)
        -> ast::Expression::Variant
    {
        return extract_qualified_lower_name_or_struct_initializer({ ast::Root_qualifier::Global {} }, context);
    }

    auto extract_self(Parse_context&)
        -> ast::Expression::Variant
    {
        return ast::expression::Self {};
    }

    auto extract_dereference(Parse_context& context)
        -> ast::Expression::Variant
    {
        return ast::expression::Dereference { utl::wrap(extract_expression(context)) };
    }

    auto extract_tuple(Parse_context& context)
        -> ast::Expression::Variant
    {
        auto expressions = extract_comma_separated_zero_or_more<parse_expression, "an expression">(context);
        context.consume_required(Token::Type::paren_close);
        if (expressions.size() == 1)
            return std::move(expressions.front().value);
        else
            return ast::expression::Tuple { std::move(expressions) };
    }

    auto extract_array(Parse_context& context)
        -> ast::Expression::Variant
    {
        static constexpr auto extract_elements =
            extract_comma_separated_zero_or_more<parse_expression, "an array element">;

        auto elements = extract_elements(context);

        if (context.try_consume(Token::Type::bracket_close))
            return ast::expression::Array_literal { std::move(elements) };
        else if (elements.empty())
            context.error_expected("an array element or a ']'");
        else
            context.error_expected("a ',' or a ']'");
    }

    auto extract_conditional(Parse_context& context)
        -> ast::Expression::Variant
    {
        static constexpr std::string_view help =
            "the branches of a conditional expression must be block expressions";

        auto condition = extract_condition(context);

        if (auto true_branch = parse_block_expression(context)) {
            tl::optional<utl::Wrapper<ast::Expression>> false_branch;

            Token const* const else_token = context.pointer;

            if (context.try_consume(Token::Type::else_)) {
                if (auto branch = parse_block_expression(context))
                    false_branch = utl::wrap(std::move(*branch));
                else
                    context.error_expected("the false branch", help);
            }
            else if (context.try_consume(Token::Type::elif)) {
                Token const* const anchor = context.pointer;
                false_branch = utl::wrap(ast::Expression {
                    .value       = extract_conditional(context),
                    .source_view = make_source_view(anchor - 1, context.pointer - 1)
                });
            }

            if (auto* const literal = std::get_if<ast::expression::Literal<compiler::Boolean>>(&condition.value)) {
                static constexpr auto selected_if = [](bool const x) noexcept {
                    return x ? "This branch will always be selected"
                             : "This branch will never be selected";
                };

                std::vector<utl::diagnostics::Text_section> sections;
                sections.push_back({
                    .source_view = condition.source_view,
                    .source      = context.source,
                    .note        = selected_if(literal->value.value)
                });
                if (false_branch.has_value()) {
                    sections.push_back({
                        .source_view = else_token->source_view,
                        .source      = context.source,
                        .note        = selected_if(!literal->value.value)
                    });
                }

                context.compilation_info.diagnostics.emit_warning({
                    .sections = std::move(sections),
                    .message  = "Boolean literal condition"
                });
            }

            return ast::expression::Conditional {
                utl::wrap(std::move(condition)),
                utl::wrap(std::move(*true_branch)),
                std::move(false_branch)
            };
        }
        context.error_expected("the true branch", help);
    }

    auto extract_let_binding(Parse_context& context)
        -> ast::Expression::Variant
    {
        auto pattern = extract_required<parse_top_level_pattern, "a pattern">(context);
        
        tl::optional<utl::Wrapper<ast::Type>> type;
        if (context.try_consume(Token::Type::colon))
            type = utl::wrap(extract_type(context));

        context.consume_required(Token::Type::equals);

        return ast::expression::Let_binding {
            .pattern     = utl::wrap(std::move(pattern)),
            .initializer = utl::wrap(extract_expression(context)),
            .type        = type
        };
    }

    auto extract_local_type_alias(Parse_context& context)
        -> ast::Expression::Variant
    {
        auto identifier = extract_upper_id(context, "an alias name");
        context.consume_required(Token::Type::equals);
        return ast::expression::Local_type_alias {
            .identifier   = identifier,
            .aliased_type = utl::wrap(extract_type(context))
        };
    }


    auto parse_capture (Parse_context& context)
        -> tl::optional<ast::expression::Lambda::Capture>
    {
        using Capture = ast::expression::Lambda::Capture;
        Token const* const anchor = context.pointer;

        return std::invoke([&]() -> tl::optional<Capture::Variant> {
            if (context.try_consume(Token::Type::ampersand)) {
                return Capture::By_reference {
                    .variable = extract_lower_id(context, "a variable name")
                };
            }
            else if (auto pattern = parse_pattern(context)) {
                context.consume_required(Token::Type::equals);
                return Capture::By_pattern {
                    .pattern    = utl::wrap(std::move(*pattern)),
                    .expression = utl::wrap(extract_expression(context))
                };
            }
            return tl::nullopt;
        })
            .transform([&](Capture::Variant&& value)
        {
            return Capture {
                .value       = std::move(value),
                .source_view = make_source_view(anchor, context.pointer - 1)
            };
        });
    }

    auto extract_lambda(Parse_context& context)
        -> ast::Expression::Variant
    {
        auto parameters = extract_function_parameters(context);

        static constexpr auto extract_captures =
            extract_comma_separated_zero_or_more<parse_capture, "a lambda capture">;

        std::vector<ast::expression::Lambda::Capture> captures;
        if (context.try_consume(Token::Type::dot)) {
            captures = extract_captures(context);

            if (captures.empty()) {
                context.error_expected(
                    "at least one lambda capture",
                    "If the lambda isn't supposed to capture anything, "
                    "or if it only captures by move, remove the '.'");
            }
        }

        context.consume_required(Token::Type::right_arrow);
        auto body = extract_expression(context);

        return ast::expression::Lambda {
            .body              = utl::wrap(std::move(body)),
            .parameters        = std::move(parameters),
            .explicit_captures = std::move(captures)
        };
    }


    auto extract_hole(Parse_context&)
        -> ast::Expression::Variant
    {
        return ast::expression::Hole {};
    }


    auto extract_sizeof(Parse_context& context)
        -> ast::Expression::Variant
    {
        if (context.try_consume(Token::Type::paren_open)) {
            auto type = extract_type(context);
            context.consume_required(Token::Type::paren_close);
            return ast::expression::Sizeof { utl::wrap(std::move(type)) };
        }
        context.error_expected("a parenthesized type");
    }

    auto extract_addressof(Parse_context& context)
        -> ast::Expression::Variant
    {
        if (context.try_consume(Token::Type::paren_open)) {
            auto lvalue = extract_expression(context);
            context.consume_required(Token::Type::paren_close);
            return ast::expression::Addressof { utl::wrap(std::move(lvalue)) };
        }
        context.error_expected("a parenthesized addressable expression");
    }

    auto extract_unsafe_dereference(Parse_context& context)
        -> ast::Expression::Variant
    {
        if (context.try_consume(Token::Type::paren_open)) {
            auto pointer = extract_expression(context);
            context.consume_required(Token::Type::paren_close);
            return ast::expression::Unsafe_dereference { utl::wrap(std::move(pointer)) };
        }
        context.error_expected("a parenthesized pointer expression");
    }


    auto parse_match_case(Parse_context& context)
        -> tl::optional<ast::expression::Match::Case>
    {
        if (auto pattern = parse_top_level_pattern(context)) {
            context.consume_required(Token::Type::right_arrow);
            return ast::expression::Match::Case {
                utl::wrap(std::move(*pattern)),
                utl::wrap(extract_expression(context))
            };
        }
        return tl::nullopt;
    }


    auto extract_match(Parse_context& context)
        -> ast::Expression::Variant
    {
        auto expression = extract_expression(context);

        context.consume_required(Token::Type::brace_open);

        std::vector<ast::expression::Match::Case> cases;
        while (auto match_case = parse_match_case(context))
            cases.push_back(*match_case);

        if (cases.empty())
            context.error_expected("one or more match cases");

        context.consume_required(Token::Type::brace_close);

        return ast::expression::Match {
            .cases              = std::move(cases),
            .matched_expression = utl::wrap(std::move(expression))
        };
    }

    auto extract_continue(Parse_context&)
        -> ast::Expression::Variant
    {
        return ast::expression::Continue {};
    }

    auto extract_break(Parse_context& context)
        -> ast::Expression::Variant
    {
        tl::optional<ast::Name> label;

        {
            Token* const anchor = context.pointer;
            if (auto name = parse_lower_name(context); name && context.try_consume(Token::Type::loop))
                label = *name;
            else
                context.pointer = anchor;
        }

        return ast::expression::Break {
            .label  = std::move(label),
            .result = parse_expression(context).transform(utl::wrap)
        };
    }

    auto extract_ret(Parse_context& context)
        -> ast::Expression::Variant
    {
        return ast::expression::Ret { parse_expression(context).transform(utl::wrap) };
    }

    auto extract_discard(Parse_context& context)
        -> ast::Expression::Variant
    {
        return ast::expression::Discard { utl::wrap(extract_expression(context)) };
    }

    auto extract_reference(Parse_context& context)
        -> ast::Expression::Variant
    {
        auto mutability = extract_mutability(context);
        return ast::expression::Reference {
            .mutability            = std::move(mutability),
            .referenced_expression = utl::wrap(extract_expression(context))
        };
    }

    auto extract_move(Parse_context& context)
        -> ast::Expression::Variant
    {
        return ast::expression::Move { utl::wrap(extract_expression(context)) };
    }

    auto extract_meta(Parse_context& context)
        -> ast::Expression::Variant
    {
        if (context.try_consume(Token::Type::paren_open)) {
            auto expression = extract_expression(context);
            context.consume_required(Token::Type::paren_close);
            return ast::expression::Meta { utl::wrap(std::move(expression)) };
        }
        context.error_expected("a parenthesized expression");
    }

    auto extract_block_expression(Parse_context& context)
        -> ast::Expression::Variant
    {
        std::vector<ast::Expression> expressions;

        if (auto head = parse_expression(context)) {
            expressions.push_back(std::move(*head));

            while (context.try_consume(Token::Type::semicolon)) {
                if (auto expression = parse_expression(context)) {
                    expressions.push_back(std::move(*expression));
                }
                else {
                    context.consume_required(Token::Type::brace_close);
                    return ast::expression::Block { .side_effect_expressions = std::move(expressions) };
                }
            }
        }

        context.consume_required(Token::Type::brace_close);

        tl::optional<utl::Wrapper<ast::Expression>> result;
        if (!expressions.empty()) {
            result = utl::wrap(std::move(expressions.back()));
            expressions.pop_back();
        }

        return ast::expression::Block {
            .side_effect_expressions = std::move(expressions),
            .result_expression       = std::move(result)
        };
    }


    auto parse_complicated_type(Parse_context& context)
        -> tl::optional<ast::Expression::Variant>
    {
        context.retreat();
        Token const* const anchor = context.pointer;

        if (auto type = parse_type(context)) {
            if (context.try_consume(Token::Type::double_colon)) {
                return extract_qualified_lower_name_or_struct_initializer(
                    ast::Root_qualifier { utl::wrap(std::move(*type)) },
                    context);
            }
            else if (context.try_consume(Token::Type::brace_open)) {
                return extract_struct_initializer(std::move(*type), context);
            }

            context.error({ anchor, context.pointer }, "Expected an expression, but found a type");
        }
        return tl::nullopt;
    }


    auto parse_normal_expression(Parse_context& context)
        -> tl::optional<ast::Expression::Variant>
    {
        switch (context.extract().type) {
        case Token::Type::signed_integer:
            return extract_literal<compiler::Signed_integer>(context);
        case Token::Type::unsigned_integer:
            return extract_literal<compiler::Unsigned_integer>(context);
        case Token::Type::integer_of_unknown_sign:
            return extract_literal<compiler::Integer_of_unknown_sign>(context);
        case Token::Type::floating:
            return extract_literal<compiler::Floating>(context);
        case Token::Type::character:
            return extract_literal<compiler::Character>(context);
        case Token::Type::boolean:
            return extract_literal<compiler::Boolean>(context);
        case Token::Type::string:
            return extract_literal<compiler::String>(context);
        case Token::Type::lower_name:
        case Token::Type::upper_name:
            return extract_identifier(context);
        case Token::Type::lower_self:
            return extract_self(context);
        case Token::Type::double_colon:
            return extract_global_identifier(context);
        case Token::Type::asterisk:
            return extract_dereference(context);
        case Token::Type::paren_open:
            return extract_tuple(context);
        case Token::Type::bracket_open:
            return extract_array(context);
        case Token::Type::if_:
            return extract_conditional(context);
        case Token::Type::let:
            return extract_let_binding(context);
        case Token::Type::alias:
            return extract_local_type_alias(context);
        case Token::Type::lambda:
            return extract_lambda(context);
        case Token::Type::hole:
            return extract_hole(context);
        case Token::Type::loop:
        case Token::Type::while_:
        case Token::Type::for_:
            return extract_loop(context);
        case Token::Type::sizeof_:
            return extract_sizeof(context);
        case Token::Type::addressof:
            return extract_addressof(context);
        case Token::Type::unsafe_dereference:
            return extract_unsafe_dereference(context);
        case Token::Type::match:
            return extract_match(context);
        case Token::Type::continue_:
            return extract_continue(context);
        case Token::Type::break_:
            return extract_break(context);
        case Token::Type::ret:
            return extract_ret(context);
        case Token::Type::discard:
            return extract_discard(context);
        case Token::Type::ampersand:
            return extract_reference(context);
        case Token::Type::mov:
            return extract_move(context);
        case Token::Type::meta:
            return extract_meta(context);
        case Token::Type::brace_open:
            return extract_block_expression(context);
        default:
            return parse_complicated_type(context);
        }
    }


    auto parse_argument(Parse_context& context)
        -> tl::optional<ast::Function_argument>
    {
        if (auto name = parse_lower_name(context)) {
            if (context.try_consume(Token::Type::equals)) {
                return ast::Function_argument {
                    .expression = extract_expression(context),
                    .name       = std::move(*name)
                };
            }
            else {
                context.retreat();
            }
        }
        static constexpr auto mk_arg = [](ast::Expression&& expression) {
            return ast::Function_argument { .expression = std::move(expression) };
        };
        return parse_expression(context).transform(mk_arg);
    }

    auto extract_arguments(Parse_context& context)
        -> std::vector<ast::Function_argument>
    {
        auto arguments = extract_comma_separated_zero_or_more<parse_argument, "a function argument">(context);
        context.consume_required(Token::Type::paren_close);
        return arguments;
    }


    auto parse_potential_invocation(Parse_context& context)
        -> tl::optional<ast::Expression>
    {
        Token const* const anchor = context.pointer;
        auto potential_invocable = parse_node<ast::Expression, parse_normal_expression>(context);

        if (potential_invocable) {
            while (context.try_consume(Token::Type::paren_open)) {
                auto arguments = extract_arguments(context);

                *potential_invocable = ast::Expression {
                    .value = ast::expression::Invocation {
                        std::move(arguments),
                        utl::wrap(std::move(*potential_invocable))
                    },
                    .source_view = make_source_view(anchor, context.pointer - 1)
                };
            }
        }

        return potential_invocable;
    }


    auto parse_potential_member_access(Parse_context& context)
        -> tl::optional<ast::Expression>
    {
        auto* const anchor     = context.pointer;
        auto        expression = parse_potential_invocation(context);

        if (expression) {
            while (context.try_consume(Token::Type::dot)) {
                if (auto field_name = parse_lower_name(context)) {
                    auto template_arguments = parse_template_arguments(context);

                    if (context.try_consume(Token::Type::paren_open)) {
                        auto arguments = extract_arguments(context);
                        *expression = ast::Expression {
                            .value = ast::expression::Method_invocation {
                                .arguments          = std::move(arguments),
                                .template_arguments = std::move(template_arguments),
                                .base_expression    = utl::wrap(std::move(*expression)),
                                .method_name        = *field_name
                            },
                            .source_view = make_source_view(anchor, context.pointer-1)
                        };
                    }
                    else if (template_arguments.has_value()) {
                        context.error_expected("a parenthesized argument set");
                    }
                    else {
                        *expression = ast::Expression {
                            .value = ast::expression::Struct_field_access {
                                .base_expression = utl::wrap(std::move(*expression)),
                                .field_name      = *field_name
                            },
                            .source_view = make_source_view(anchor, context.pointer-1)
                        };
                    }
                }
                else if (context.pointer->type == Token::Type::integer_of_unknown_sign
                      || context.pointer->type == Token::Type::unsigned_integer)
                {
                    Token const& field_index_token = context.extract();
                    *expression = ast::Expression {
                        .value = ast::expression::Tuple_field_access {
                            .base_expression         = utl::wrap(std::move(*expression)),
                            .field_index             = field_index_token.as_unsigned_integer(),
                            .field_index_source_view = field_index_token.source_view
                        },
                        .source_view = make_source_view(anchor, context.pointer-1)
                    };
                }
                else if (context.try_consume(Token::Type::bracket_open)) {
                    auto index_expression = extract_expression(context);
                    context.consume_required(Token::Type::bracket_close);
                    *expression = ast::Expression {
                        .value = ast::expression::Array_index_access {
                            .base_expression = utl::wrap(std::move(*expression)),
                            .index_expression = utl::wrap(std::move(index_expression))
                        },
                        .source_view = make_source_view(anchor, context.pointer-1)
                    };
                }
                else {
                    context.error_expected("a struct member name (a.b), a tuple member index (a.0), or an array index (a.[b])");
                }
            }
        }

        return expression;
    }


    auto parse_potential_type_cast(Parse_context& context)
        -> tl::optional<ast::Expression>
    {
        Token const* const anchor = context.pointer;

        if (auto expression = parse_potential_member_access(context)) {
            bool continue_looping = true;

            while (continue_looping) {
                using Kind = ast::expression::Type_cast::Kind;

                Kind cast_kind = Kind::conversion;

                switch (context.extract().type) {
                case Token::Type::colon:
                    cast_kind = Kind::ascription;
                    [[fallthrough]];
                case Token::Type::as:
                {
                    auto type = extract_type(context);
                    *expression = ast::Expression {
                        .value = ast::expression::Type_cast {
                            .expression  = utl::wrap(std::move(*expression)),
                            .target_type = utl::wrap(std::move(type)),
                            .cast_kind   = cast_kind
                        },
                        .source_view = make_source_view(anchor, context.pointer - 1)
                    };
                    break;
                }
                default:
                    context.retreat();
                    continue_looping = false;
                }
            }

            return expression;
        }
        return tl::nullopt;
    }


    constexpr std::tuple precedence_table {
        std::to_array<std::string_view>({ "*", "/", "%" }),
        std::to_array<std::string_view>({ "+", "-" }),
        std::to_array<std::string_view>({ "?=", "!=" }),
        std::to_array<std::string_view>({ "<", "<=", ">=", ">" }),
        std::to_array<std::string_view>({ "&&", "||" }),
        std::to_array<std::string_view>({ ":=", "+=", "*=", "/=", "%=" })
    };

    constexpr utl::Usize lowest_precedence = std::tuple_size_v<decltype(precedence_table)> - 1;


    auto parse_operator(Parse_context& context)
        -> tl::optional<compiler::Identifier>
    {
        switch (context.extract().type) {
        case Token::Type::operator_name:
            return context.previous().as_identifier();
        case Token::Type::asterisk:
            return context.asterisk_id;
        case Token::Type::plus:
            return context.plus_id;
        default:
            context.retreat();
            return tl::nullopt;
        }
    }

    template <utl::Usize precedence>
    auto parse_binary_operator_invocation_with_precedence(Parse_context& context)
        -> tl::optional<ast::Expression>
    {
        static constexpr auto recurse =
            parse_binary_operator_invocation_with_precedence<precedence - 1>;

        Token const* const anchor = context.pointer;

        if (auto left = recurse(context)) {
            while (auto op = parse_operator(context)) {
                if constexpr (precedence != lowest_precedence) {
                    static constexpr auto& ops = std::get<precedence>(precedence_table);
                    if (ranges::find(ops, op->view()) == ops.end()) {
                        // The operator that was found is not in the current precedence group
                        context.retreat();
                        return left;
                    }
                }
                if (auto right = recurse(context)) {
                    *left = ast::Expression {
                        .value = ast::expression::Binary_operator_invocation {
                            utl::wrap(std::move(*left)),
                            utl::wrap(std::move(*right)),
                            *op
                        },
                        .source_view = make_source_view(anchor, context.pointer - 1)
                    };
                }
                else {
                    context.error_expected("an operand");
                }
            }
            return left;
        }
        return tl::nullopt;
    }

    template <>
    auto parse_binary_operator_invocation_with_precedence<static_cast<utl::Usize>(-1)>(Parse_context& context)
        -> tl::optional<ast::Expression>
    {
        return parse_potential_type_cast(context);
    }


    auto parse_potential_placement_init(Parse_context& context)
        -> tl::optional<ast::Expression::Variant>
    {
        if (auto expression = parse_binary_operator_invocation_with_precedence<lowest_precedence>(context)) {
            if (context.try_consume(Token::Type::left_arrow)) {
                return ast::expression::Placement_init {
                    .lvalue      = utl::wrap(std::move(*expression)),
                    .initializer = utl::wrap(extract_expression(context))
                };
            }
            return std::move(expression->value);
        }
        return tl::nullopt;
    }

}


auto parse_expression(Parse_context& context) -> tl::optional<ast::Expression> {
    return parse_node<ast::Expression, parse_potential_placement_init>(context);
}

auto parse_block_expression(Parse_context& context) -> tl::optional<ast::Expression> {
    if (context.try_consume(Token::Type::brace_open)) {
        Token const* const anchor = context.pointer;
        return ast::Expression {
            .value       = extract_block_expression(context),
            .source_view = make_source_view(anchor - 1, context.pointer - 1)
        };
    }
    return tl::nullopt;
}
