#include <libparse/parser_internals.hpp>
#include <libutl/common/utilities.hpp>

namespace {

    using namespace libparse;

    template <class T>
    auto extract_literal(Context& context) -> cst::Expression::Variant
    {
        return context.previous().value_as<T>();
    }

    auto extract_string_literal(Context& context) -> cst::Expression::Variant
    {
        auto const first_string = context.previous().value_as<kieli::String>();
        if (context.pointer->type != Token_type::string_literal) {
            return kieli::String { first_string };
        }
        std::string combined_string { first_string.value.view() };
        while (Lexical_token const* const token = context.try_extract(Token_type::string_literal)) {
            combined_string.append(token->value_as<kieli::String>().value.view());
        }
        return kieli::String {
            context.compile_info.string_literal_pool.make(combined_string),
        };
    }

    auto parse_struct_member_initializer(Context& context)
        -> std::optional<cst::expression::Struct_initializer::Member_initializer>
    {
        if (auto member_name = parse_lower_name(context)) {
            Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);
            return cst::expression::Struct_initializer::Member_initializer {
                .name              = member_name.value(),
                .expression        = extract_expression(context),
                .equals_sign_token = cst::Token::from_lexical(equals_sign),
            };
        }
        return std::nullopt;
    }

    auto extract_struct_initializer(Context& context, utl::Wrapper<cst::Type> const type)
        -> cst::Expression::Variant
    {
        static constexpr auto extract_member_initializers = extract_comma_separated_zero_or_more<
            parse_struct_member_initializer,
            "a member initializer">;

        Lexical_token const* const open         = context.pointer - 1;
        auto                       initializers = extract_member_initializers(context);
        Lexical_token const* const close        = context.extract_required(Token_type::brace_close);

        return cst::expression::Struct_initializer {
            .member_initializers {
                .value       = std::move(initializers),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            },
            .struct_type = type,
        };
    }

    auto extract_condition(Context& context) -> utl::Wrapper<cst::Expression>
    {
        Lexical_token const* const anchor = context.pointer;
        if (Lexical_token const* const let_keyword = context.try_extract(Token_type::let)) {
            utl::wrapper auto const    pattern     = extract_pattern(context);
            Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);
            return context.wrap(cst::Expression {
                .value = cst::expression::Conditional_let {
                    .pattern           = pattern,
                    .initializer       = extract_expression(context),
                    .let_keyword_token = cst::Token::from_lexical(let_keyword),
                    .equals_sign_token = cst::Token::from_lexical(equals_sign),
                },
                .source_view = context.make_source_view(anchor, context.pointer - 1),
            });
        }
        return extract_expression(context);
    }

    auto extract_loop_body(Context& context) -> utl::Wrapper<cst::Expression>
    {
        if (auto const body = parse_block_expression(context)) {
            return body.value();
        }
        context.error_expected("the loop body, which must be a block expression");
    }

    auto extract_infinite_loop(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const loop_keyword = context.pointer - 1;
        assert(loop_keyword->source_view.string == "loop");
        return cst::expression::Infinite_loop {
            .body               = extract_loop_body(context),
            .loop_keyword_token = cst::Token::from_lexical(loop_keyword),
        };
    }

    auto extract_while_loop(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const while_keyword = context.pointer - 1;
        assert(while_keyword->source_view.string == "while");
        utl::wrapper auto const condition = extract_condition(context);
        return cst::expression::While_loop {
            .condition           = condition,
            .body                = extract_loop_body(context),
            .while_keyword_token = cst::Token::from_lexical(while_keyword),
        };
    }

    auto extract_for_loop(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const for_keyword = context.pointer - 1;
        utl::wrapper auto const    iterator    = extract_pattern(context);
        Lexical_token const* const in_keyword  = context.extract_required(Token_type::in);
        utl::wrapper auto const    iterable    = extract_expression(context);
        assert(for_keyword->source_view.string == "for");
        return cst::expression::For_loop {
            .iterator          = iterator,
            .iterable          = iterable,
            .body              = extract_loop_body(context),
            .for_keyword_token = cst::Token::from_lexical(for_keyword),
            .in_keyword_token  = cst::Token::from_lexical(in_keyword),
        };
    }

    auto extract_qualified_lower_name_or_struct_initializer(
        Context& context, std::optional<cst::Root_qualifier>&& root) -> cst::Expression::Variant
    {
        Lexical_token const* const anchor = context.pointer;
        auto                       name   = extract_qualified(context, std::move(root));

        auto template_arguments = parse_template_arguments(context);

        if (!name.is_upper()) {
            if (template_arguments) {
                return cst::expression::Template_application {
                    .template_arguments = std::move(template_arguments.value()),
                    .name               = std::move(name),
                };
            }
            return cst::expression::Variable { std::move(name) };
        }
        if (context.try_consume(Token_type::brace_open)) {
            auto value = std::invoke([&]() -> cst::Type::Variant {
                if (template_arguments) {
                    return cst::type::Template_application {
                        .template_arguments = std::move(template_arguments.value()),
                        .name               = std::move(name),
                    };
                }
                return cst::type::Typename { std::move(name) };
            });
            return extract_struct_initializer(
                context,
                context.wrap(cst::Type {
                    .value       = std::move(value),
                    .source_view = context.make_source_view(anchor, context.pointer - 1),
                }));
        }
        context.diagnostics().error(
            context.make_source_view(anchor, context.pointer - 1),
            "Expected an expression, but found a type");
    }

    auto extract_identifier(Context& context) -> cst::Expression::Variant
    {
        context.retreat();
        return extract_qualified_lower_name_or_struct_initializer(context, {});
    }

    auto extract_global_identifier(Context& context) -> cst::Expression::Variant
    {
        assert(context.pointer[-1].source_view.string == "global");
        return extract_qualified_lower_name_or_struct_initializer(
            context,
            cst::Root_qualifier {
                .value = cst::Root_qualifier::Global {},
                .double_colon_token
                = cst::Token::from_lexical(context.extract_required(Token_type::double_colon)),
            });
    }

    auto extract_reference_dereference(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const asterisk = context.pointer - 1;
        assert(asterisk->source_view.string == "*");
        return cst::expression::Reference_dereference {
            .dereferenced_expression = extract_expression(context),
            .asterisk_token          = cst::Token::from_lexical(asterisk),
        };
    }

    auto extract_tuple(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const open = context.pointer - 1;
        assert(open->source_view.string == "(");
        auto expressions
            = extract_comma_separated_zero_or_more<parse_expression, "an expression">(context);
        Lexical_token const* const close = context.extract_required(Token_type::paren_close);
        if (expressions.elements.size() == 1) {
            return cst::expression::Parenthesized { {
                .value       = expressions.elements.front(),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            } };
        }
        return cst::expression::Tuple { {
            .value       = std::move(expressions),
            .open_token  = cst::Token::from_lexical(open),
            .close_token = cst::Token::from_lexical(close),
        } };
    }

    auto extract_array(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const open = context.pointer - 1;
        assert(open->source_view.string == "[");
        auto elements
            = extract_comma_separated_zero_or_more<parse_expression, "an array element">(context);
        if (Lexical_token const* const close = context.try_extract(Token_type::bracket_close)) {
            return cst::expression::Array_literal { {
                .value       = std::move(elements),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            } };
        }
        if (elements.elements.empty()) {
            context.error_expected("an array element or a ']'");
        }
        else {
            context.error_expected("a ',' or a ']'");
        }
    }

    enum class Conditional_kind { if_, elif };

    auto extract_conditional(Context& context, Conditional_kind const kind)
        -> cst::Expression::Variant
    {
        static constexpr auto extract_branch = [](Context& context) -> utl::wrapper auto {
            if (auto branch = parse_block_expression(context)) {
                return branch.value();
            }
            context.error_expected("a block expression");
        };

        Lexical_token const* const if_keyword = context.pointer - 1;
        assert(if_keyword->source_view.string == "if" || if_keyword->source_view.string == "elif");

        utl::wrapper auto const primary_condition = extract_condition(context);
        utl::wrapper auto const true_branch       = extract_branch(context);

        std::optional<cst::expression::Conditional::False_branch> false_branch;

        if (Lexical_token const* const elif_keyword = context.try_extract(Token_type::elif)) {
            auto elif_conditional = extract_conditional(context, Conditional_kind::elif);

            false_branch = cst::expression::Conditional::False_branch {
                .body = context.wrap(cst::Expression {
                    .value       = std::move(elif_conditional),
                    .source_view = context.make_source_view(elif_keyword, context.pointer - 1),
                }),

                .else_or_elif_keyword_token = cst::Token::from_lexical(elif_keyword),
            };
        }
        else if (Lexical_token const* const else_keyword = context.try_extract(Token_type::else_)) {
            false_branch = cst::expression::Conditional::False_branch {
                .body                       = extract_branch(context),
                .else_or_elif_keyword_token = cst::Token::from_lexical(else_keyword),
            };
        }

        return cst::expression::Conditional {
            .condition                = primary_condition,
            .true_branch              = true_branch,
            .false_branch             = false_branch,
            .if_or_elif_keyword_token = cst::Token::from_lexical(if_keyword),
            .is_elif_conditional      = kind == Conditional_kind::elif,
        };
    }

    auto extract_let_binding(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const let_keyword = context.pointer - 1;
        auto pattern = extract_required<parse_top_level_pattern, "a pattern">(context);
        auto type    = parse_type_annotation(context);
        Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);
        assert(let_keyword->source_view.string == "let");
        return cst::expression::Let_binding {
            .pattern           = std::move(pattern),
            .type              = type,
            .initializer       = extract_expression(context),
            .let_keyword_token = cst::Token::from_lexical(let_keyword),
            .equals_sign_token = cst::Token::from_lexical(equals_sign),
        };
    }

    auto extract_local_type_alias(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const alias_keyword = context.pointer - 1;
        assert(alias_keyword->source_view.string == "alias");
        auto                       name        = extract_upper_name(context, "an alias name");
        Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);
        return cst::expression::Local_type_alias {
            .alias_name          = std::move(name),
            .aliased_type        = extract_type(context),
            .alias_keyword_token = cst::Token::from_lexical(alias_keyword),
            .equals_sign_token   = cst::Token::from_lexical(equals_sign),
        };
    }

    auto extract_sizeof(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const sizeof_keyword = context.pointer - 1;
        assert(sizeof_keyword->source_view.string == "sizeof");
        if (auto type = parenthesized<parse_type, "a type">(context)) {
            return cst::expression::Sizeof {
                .inspected_type       = type.value(),
                .sizeof_keyword_token = cst::Token::from_lexical(sizeof_keyword),
            };
        }
        context.error_expected("a parenthesized type");
    }

    auto extract_addressof(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const addressof_keyword = context.pointer - 1;
        assert(addressof_keyword->source_view.string == "addressof");
        if (auto lvalue = parenthesized<parse_expression, "an addressable expression">(context)) {
            return cst::expression::Addressof {
                .lvalue_expression       = lvalue.value(),
                .addressof_keyword_token = cst::Token::from_lexical(addressof_keyword),
            };
        }
        context.error_expected("a parenthesized addressable expression");
    }

    auto extract_pointer_dereference(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const dereference_keyword = context.pointer - 1;
        assert(dereference_keyword->source_view.string == "dereference");
        if (auto pointer_expression
            = parenthesized<parse_expression, "a pointer expression">(context))
        {
            return cst::expression::Pointer_dereference {
                .pointer_expression        = std::move(pointer_expression.value()),
                .dereference_keyword_token = cst::Token::from_lexical(dereference_keyword),
            };
        }
        context.error_expected("a parenthesized pointer expression");
    }

    auto parse_match_case(Context& context) -> std::optional<cst::expression::Match::Case>
    {
        if (auto pattern = parse_top_level_pattern(context)) {
            Lexical_token const* const arrow   = context.extract_required(Token_type::right_arrow);
            utl::wrapper auto const    handler = extract_expression(context);
            Lexical_token const* const semicolon = context.try_extract(Token_type::semicolon);
            return cst::expression::Match::Case {
                .pattern                  = std::move(pattern.value()),
                .handler                  = handler,
                .arrow_token              = cst::Token::from_lexical(arrow),
                .optional_semicolon_token = optional_token(semicolon),
            };
        }
        return std::nullopt;
    }

    auto extract_match(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const match_keyword = context.pointer - 1;
        auto                       expression    = extract_expression(context);
        Lexical_token const* const open          = context.extract_required(Token_type::brace_open);
        assert(match_keyword->source_view.string == "match");

        std::vector<cst::expression::Match::Case> cases;
        while (auto match_case = parse_match_case(context)) {
            cases.push_back(std::move(match_case.value()));
        }
        if (cases.empty()) {
            context.error_expected("one or more match cases");
        }

        Lexical_token const* const close = context.extract_required(Token_type::brace_close);
        return cst::expression::Match {
            .cases {
                .value       = std::move(cases),
                .open_token  = cst::Token::from_lexical(open),
                .close_token = cst::Token::from_lexical(close),
            },
            .matched_expression  = std::move(expression),
            .match_keyword_token = cst::Token::from_lexical(match_keyword),
        };
    }

    auto extract_continue(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const continue_keyword = context.pointer - 1;
        assert(continue_keyword->source_view.string == "continue");
        return cst::expression::Continue {
            .continue_keyword_token = cst::Token::from_lexical(continue_keyword),
        };
    }

    auto extract_break(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const break_keyword = context.pointer - 1;
        assert(break_keyword->source_view.string == "break");
        return cst::expression::Break {
            .result              = parse_expression(context),
            .break_keyword_token = cst::Token::from_lexical(break_keyword),
        };
    }

    auto extract_ret(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const ret_keyword = context.pointer - 1;
        assert(ret_keyword->source_view.string == "ret");
        return cst::expression::Ret {
            .returned_expression = parse_expression(context),
            .ret_keyword_token   = cst::Token::from_lexical(ret_keyword),
        };
    }

    auto extract_discard(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const discard_keyword = context.pointer - 1;
        assert(discard_keyword->source_view.string == "discard");
        return cst::expression::Discard {
            .discarded_expression  = extract_expression(context),
            .discard_keyword_token = cst::Token::from_lexical(discard_keyword),
        };
    }

    auto extract_reference(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const ampersand = context.pointer - 1;
        assert(ampersand->source_view.string == "&");
        auto mutability = parse_mutability(context);
        return cst::expression::Reference {
            .mutability            = std::move(mutability),
            .referenced_expression = extract_expression(context),
            .ampersand_token       = cst::Token::from_lexical(ampersand),
        };
    }

    auto extract_unsafe_block(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const unsafe_keyword = context.pointer - 1;
        assert(unsafe_keyword->source_view.string == "unsafe");
        if (auto block = parse_block_expression(context)) {
            return cst::expression::Unsafe {
                .expression           = std::move(block.value()),
                .unsafe_keyword_token = cst::Token::from_lexical(unsafe_keyword),
            };
        }
        context.error_expected("an unsafe block expression");
    }

    auto extract_move(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const mov_keyword = context.pointer - 1;
        assert(mov_keyword->source_view.string == "mov");
        return cst::expression::Move {
            .lvalue            = extract_expression(context),
            .mov_keyword_token = cst::Token::from_lexical(mov_keyword),
        };
    }

    auto extract_meta(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const meta_keyword = context.pointer - 1;
        assert(meta_keyword->source_view.string == "meta");
        if (auto expression = parenthesized<parse_expression, "an expression">(context)) {
            return cst::expression::Meta {
                .expression         = expression.value(),
                .meta_keyword_token = cst::Token::from_lexical(meta_keyword),
            };
        }
        context.error_expected("a parenthesized expression");
    }

    auto extract_block_expression(Context& context) -> cst::Expression::Variant
    {
        Lexical_token const* const                       open_brace = context.pointer - 1;
        std::vector<cst::expression::Block::Side_effect> side_effects;
        std::optional<utl::Wrapper<cst::Expression>>     result_expression;
        assert(open_brace->source_view.string == "{");

        while (auto expression = parse_expression(context)) {
            if (Lexical_token const* const semicolon = context.try_extract(Token_type::semicolon)) {
                side_effects.push_back(cst::expression::Block::Side_effect {
                    .expression               = expression.value(),
                    .trailing_semicolon_token = cst::Token::from_lexical(semicolon),
                });
            }
            else {
                result_expression = expression.value();
                break;
            }
        }

        Lexical_token const* const close_brace = context.extract_required(Token_type::brace_close);
        return cst::expression::Block {
            .side_effects      = std::move(side_effects),
            .result_expression = std::move(result_expression),
            .open_brace_token  = cst::Token::from_lexical(open_brace),
            .close_brace_token = cst::Token::from_lexical(close_brace),
        };
    }

    auto parse_complicated_type(Context& context) -> std::optional<cst::Expression::Variant>
    {
        context.retreat();
        Lexical_token const* const anchor = context.pointer;
        if (auto const type = parse_type(context)) {
            if (Lexical_token const* const double_colon
                = context.try_extract(Token_type::double_colon))
            {
                return extract_qualified_lower_name_or_struct_initializer(
                    context,
                    cst::Root_qualifier {
                        .value              = type.value(),
                        .double_colon_token = cst::Token::from_lexical(double_colon),
                    });
            }
            if (context.try_consume(Token_type::brace_open)) {
                return extract_struct_initializer(context, type.value());
            }
            context.diagnostics().error(
                context.make_source_view(anchor, context.pointer),
                "Expected an expression, but found a type");
        }
        return std::nullopt;
    }

    auto parse_normal_expression(Context& context) -> std::optional<cst::Expression::Variant>
    {
        // clang-format off
        switch (context.extract().type) {
        case Token_type::integer_literal:   return extract_literal<kieli::Integer>(context);
        case Token_type::floating_literal:  return extract_literal<kieli::Floating>(context);
        case Token_type::character_literal: return extract_literal<kieli::Character>(context);
        case Token_type::boolean_literal:   return extract_literal<kieli::Boolean>(context);
        case Token_type::string_literal:    return extract_string_literal(context);
        case Token_type::lower_name:        [[fallthrough]];
        case Token_type::upper_name:        return extract_identifier(context);
        case Token_type::global:            return extract_global_identifier(context);
        case Token_type::lower_self:        return cst::expression::Self {};
        case Token_type::hole:              return cst::expression::Hole {};
        case Token_type::asterisk:          return extract_reference_dereference(context);
        case Token_type::paren_open:        return extract_tuple(context);
        case Token_type::bracket_open:      return extract_array(context);
        case Token_type::if_:               return extract_conditional(context, Conditional_kind::if_);
        case Token_type::let:               return extract_let_binding(context);
        case Token_type::alias:             return extract_local_type_alias(context);
        case Token_type::loop:              return extract_infinite_loop(context);
        case Token_type::while_:            return extract_while_loop(context);
        case Token_type::for_:              return extract_for_loop(context);
        case Token_type::sizeof_:           return extract_sizeof(context);
        case Token_type::addressof:         return extract_addressof(context);
        case Token_type::dereference:       return extract_pointer_dereference(context);
        case Token_type::unsafe:            return extract_unsafe_block(context);
        case Token_type::match:             return extract_match(context);
        case Token_type::continue_:         return extract_continue(context);
        case Token_type::break_:            return extract_break(context);
        case Token_type::ret:               return extract_ret(context);
        case Token_type::discard:           return extract_discard(context);
        case Token_type::ampersand:         return extract_reference(context);
        case Token_type::mov:               return extract_move(context);
        case Token_type::meta:              return extract_meta(context);
        case Token_type::brace_open:        return extract_block_expression(context);
        default:                            return parse_complicated_type(context);
            // clang-format on
        }
    }

    auto parse_argument(Context& context) -> std::optional<cst::Function_argument>
    {
        if (auto const name = parse_lower_name(context)) {
            if (Lexical_token const* const equals_sign = context.try_extract(Token_type::equals)) {
                return cst::Function_argument {
                    .argument_name = cst::Name_lower_equals {
                        .name              = name.value(),
                        .equals_sign_token = cst::Token::from_lexical(equals_sign),
                    },
                    .expression = extract_expression(context),
                };
            }
            context.retreat();
        }
        return parse_expression(context).transform(
            [](utl::Wrapper<cst::Expression> const expression) {
                return cst::Function_argument { .expression = expression };
            });
    }

    auto extract_arguments(Context& context) -> cst::Function_arguments
    {
        Lexical_token const* const open = context.pointer - 1;
        assert(open->source_view.string == "(");
        auto arguments
            = extract_comma_separated_zero_or_more<parse_argument, "a function argument">(context);
        Lexical_token const* const close = context.extract_required(Token_type::paren_close);
        return cst::Function_arguments {
            .value       = std::move(arguments),
            .open_token  = cst::Token::from_lexical(open),
            .close_token = cst::Token::from_lexical(close),
        };
    }

    auto parse_potential_invocation(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        Lexical_token const* const anchor = context.pointer;
        auto potential_invocable = parse_node<cst::Expression, parse_normal_expression>(context);
        if (potential_invocable) {
            while (context.try_consume(Token_type::paren_open)) {
                auto arguments = extract_arguments(context);
                potential_invocable.value() = context.wrap(cst::Expression {
                    .value = cst::expression::Invocation {
                        .function_arguments  = std::move(arguments),
                        .function_expression = std::move(potential_invocable.value()),
                    },
                    .source_view = context.make_source_view(anchor, context.pointer - 1),
                });
            }
        }
        return potential_invocable;
    }

    auto extract_struct_field_access(
        kieli::Name_lower const             field_name,
        cst::Token&&                        dot_token,
        utl::Wrapper<cst::Expression> const expression,
        Context&                            context) -> cst::Expression::Variant
    {
        auto template_arguments = parse_template_arguments(context);
        if (context.try_consume(Token_type::paren_open)) {
            return cst::expression::Method_invocation {
                .function_arguments = extract_arguments(context),
                .template_arguments = std::move(template_arguments),
                .base_expression    = expression,
                .method_name        = field_name,
            };
        }
        if (template_arguments.has_value()) {
            context.error_expected("a parenthesized argument set");
        }
        return cst::expression::Struct_field_access {
            .base_expression = expression,
            .field_name      = field_name,
            .dot_token       = std::move(dot_token),
        };
    }

    auto extract_tuple_field_access(
        cst::Token&&                        dot_token,
        utl::Wrapper<cst::Expression> const expression,
        Context&                            context) -> cst::Expression::Variant
    {
        Lexical_token const& field_index_token = context.extract();
        return cst::expression::Tuple_field_access {
            .base_expression         = expression,
            .field_index             = field_index_token.value_as<kieli::Integer>().value,
            .field_index_source_view = field_index_token.source_view,
            .dot_token               = std::move(dot_token),
        };
    }

    auto extract_array_index_access(
        cst::Token&&                        dot_token,
        utl::Wrapper<cst::Expression> const expression,
        Context&                            context) -> cst::Expression::Variant
    {
        utl::wrapper auto const index_expression = extract_expression(context);
        context.consume_required(Token_type::bracket_close);
        return cst::expression::Array_index_access {
            .base_expression  = expression,
            .index_expression = index_expression,
            .dot_token        = std::move(dot_token),
        };
    }

    auto extract_member_access(
        cst::Token&&                        dot_token,
        utl::Wrapper<cst::Expression> const expression,
        Context&                            context) -> cst::Expression::Variant
    {
        if (auto field_name = parse_lower_name(context)) {
            return extract_struct_field_access(
                field_name.value(), std::move(dot_token), expression, context);
        }
        if (context.pointer->type == Token_type::integer_literal) {
            return extract_tuple_field_access(std::move(dot_token), expression, context);
        }
        if (context.try_consume(Token_type::bracket_open)) {
            return extract_array_index_access(std::move(dot_token), expression, context);
        }
        context.error_expected(
            "a struct member name (a.b), a tuple member index (a.0), or an array index (a.[b])");
    }

    auto parse_potential_member_access(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        Lexical_token const* const                   anchor = context.pointer;
        std::optional<utl::Wrapper<cst::Expression>> expression
            = parse_potential_invocation(context);
        if (expression) {
            while (Lexical_token const* const dot = context.try_extract(Token_type::dot)) {
                expression = context.wrap(cst::Expression {
                    .value = extract_member_access(
                        cst::Token::from_lexical(dot), expression.value(), context),
                    .source_view = context.make_source_view(anchor, context.pointer - 1),
                });
            }
        }
        return expression;
    }

    auto parse_potential_type_cast(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
    {
        auto const make_cast = [&, anchor = context.pointer](cst::Expression::Variant&& value) {
            return context.wrap(cst::Expression {
                .value       = std::move(value),
                .source_view = context.make_source_view(anchor, context.pointer - 1),
            });
        };
        if (auto expression = parse_potential_member_access(context)) {
            for (;;) {
                switch (context.extract().type) {
                case Token_type::colon:
                    expression = make_cast(cst::expression::Type_ascription {
                        .base_expression = expression.value(),
                        .ascribed_type   = extract_type(context),
                    });
                    break;
                case Token_type::as:
                    expression = make_cast(cst::expression::Type_cast {
                        .base_expression = expression.value(),
                        .target_type     = extract_type(context),
                    });
                    break;
                default:
                    context.retreat();
                    return expression;
                }
            }
        }
        return std::nullopt;
    }

    auto parse_operator(Context& context) -> std::optional<kieli::Identifier>
    {
        switch (context.extract().type) {
        case Token_type::operator_name:
            return context.previous().value_as<kieli::Identifier>();
        case Token_type::asterisk:
            return context.asterisk_id;
        case Token_type::plus:
            return context.plus_id;
        default:
            context.retreat();
            return std::nullopt;
        }
    }

    auto parse_binary_operator_invocation_sequence(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        Lexical_token const* const anchor = context.pointer;
        if (auto leftmost_operand = parse_potential_type_cast(context)) {
            decltype(cst::expression::Binary_operator_invocation_sequence::sequence_tail) tail;
            while (auto const operator_id = parse_operator(context)) {
                Lexical_token const* const op_token = context.pointer - 1;
                if (auto right_operand = parse_potential_type_cast(context)) {
                    tail.push_back({
                        .operator_id    = operator_id.value(),
                        .operator_token = cst::Token::from_lexical(op_token),
                        .right_operand  = right_operand.value(),
                    });
                }
                else {
                    context.error_expected("an operand");
                }
            }
            if (tail.empty()) {
                return leftmost_operand;
            }
            return context.wrap(cst::Expression {
                .value = cst::expression::Binary_operator_invocation_sequence {
                    .sequence_tail    = std::move(tail),
                    .leftmost_operand = leftmost_operand.value(),
                },
                .source_view = context.make_source_view(anchor, context.pointer - 1),
            });
        }
        return std::nullopt;
    }

} // namespace

auto libparse::parse_expression(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
{
    return parse_binary_operator_invocation_sequence(context);
}

auto libparse::parse_block_expression(Context& context)
    -> std::optional<utl::Wrapper<cst::Expression>>
{
    if (context.try_consume(Token_type::brace_open)) {
        Lexical_token const* const anchor = context.pointer;
        return context.wrap(cst::Expression {
            .value       = extract_block_expression(context),
            .source_view = context.make_source_view(anchor - 1, context.pointer - 1),
        });
    }
    return std::nullopt;
}
