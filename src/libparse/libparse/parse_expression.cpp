#include <libutl/common/utilities.hpp>
#include <libparse/parser_internals.hpp>

namespace {

    using namespace libparse;

    auto extract_string_literal(Context& context, Token const& token) -> cst::Expression::Variant
    {
        auto const first_string = token.value_as<kieli::String>();
        if (context.peek().type != Token::Type::string_literal) {
            return first_string;
        }
        std::string combined_string { first_string.value.view() };
        while (auto const token = context.try_extract(Token::Type::string_literal)) {
            combined_string.append(token.value().value_as<kieli::String>().value.view());
        }
        return kieli::String { context.compile_info().string_literal_pool.make(combined_string) };
    }

    auto extract_condition(Context& context) -> utl::Wrapper<cst::Expression>
    {
        if (auto const let_keyword = context.try_extract(Token::Type::let)) {
            auto const pattern     = require<parse_pattern>(context, "a pattern");
            auto const equals_sign = context.require_extract(Token::Type::equals);
            return context.wrap(cst::Expression {
                .value { cst::expression::Conditional_let {
                    .pattern     = pattern,
                    .initializer = require<parse_expression>(context, "the initializer expression"),
                    .let_keyword_token = cst::Token::from_lexical(let_keyword.value()),
                    .equals_sign_token = cst::Token::from_lexical(equals_sign),
                } },
                .source_range = context.up_to_current(let_keyword.value().source_range),
            });
        }
        return require<parse_expression>(context, "the condition expression");
    }

    auto extract_loop_body(Context& context) -> utl::Wrapper<cst::Expression>
    {
        return require<parse_block_expression>(
            context, "the loop body, which must be a block expression");
    }

    auto extract_infinite_loop(Context& context, Token const& loop_keyword)
        -> cst::Expression::Variant
    {
        return cst::expression::Infinite_loop {
            .body               = extract_loop_body(context),
            .loop_keyword_token = cst::Token::from_lexical(loop_keyword),
        };
    }

    auto extract_while_loop(Context& context, Token const& while_keyword)
        -> cst::Expression::Variant
    {
        return cst::expression::While_loop {
            .condition           = extract_condition(context),
            .body                = extract_loop_body(context),
            .while_keyword_token = cst::Token::from_lexical(while_keyword),
        };
    }

    auto extract_for_loop(Context& context, Token const& for_keyword) -> cst::Expression::Variant
    {
        auto const  iterator   = require<parse_pattern>(context, "a pattern");
        Token const in_keyword = context.require_extract(Token::Type::in);
        return cst::expression::For_loop {
            .iterator          = iterator,
            .iterable          = require<parse_expression>(context, "an iterable expression"),
            .body              = extract_loop_body(context),
            .for_keyword_token = cst::Token::from_lexical(for_keyword),
            .in_keyword_token  = cst::Token::from_lexical(in_keyword),
        };
    }

    auto parse_struct_member_initializer(Context& context)
        -> std::optional<cst::expression::Struct_initializer::Member_initializer>
    {
        return parse_lower_name(context).transform([&](kieli::Name_lower const name) {
            Token const equals_sign = context.require_extract(Token::Type::equals);
            return cst::expression::Struct_initializer::Member_initializer {
                .name       = name,
                .expression = require<parse_expression>(context, "an initializer expression"),
                .equals_sign_token = cst::Token::from_lexical(equals_sign),
            };
        });
    }

    auto extract_struct_initializer(
        Context&                      context,
        Token const&                  brace_open,
        utl::Wrapper<cst::Type> const struct_type) -> cst::Expression::Variant
    {
        static constexpr auto extract_member_initializers = extract_comma_separated_zero_or_more<
            parse_struct_member_initializer,
            "a member initializer">;

        auto        initializers = extract_member_initializers(context);
        Token const brace_close  = context.require_extract(Token::Type::brace_close);

        return cst::expression::Struct_initializer {
            .member_initializers {
                .value       = std::move(initializers),
                .open_token  = cst::Token::from_lexical(brace_open),
                .close_token = cst::Token::from_lexical(brace_close),
            },
            .struct_type = struct_type,
        };
    }

    auto extract_qualified_lower_name_or_struct_initializer(
        Context& context, std::optional<cst::Root_qualifier>&& root) -> cst::Expression::Variant
    {
        auto name               = extract_qualified_name(context, std::move(root));
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
        if (auto const brace_open = context.try_extract(Token::Type::brace_open)) {
            auto type = std::invoke([&]() -> cst::Type::Variant {
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
                brace_open.value(),
                context.wrap(cst::Type {
                    .value        = std::move(type),
                    .source_range = context.up_to_current(name.source_range),
                }));
        }
        context.compile_info().diagnostics.error(
            context.source(),
            name.primary_name.source_range,
            "Expected an expression, but found a type");
    }

    auto extract_identifier(Context& context, Stage const stage) -> cst::Expression::Variant
    {
        context.unstage(stage);
        return extract_qualified_lower_name_or_struct_initializer(context, std::nullopt);
    }

    auto extract_global_identifier(Context& context, Token const& global)
        -> cst::Expression::Variant
    {
        return extract_qualified_lower_name_or_struct_initializer(
            context,
            cst::Root_qualifier {
                .value = cst::Root_qualifier::Global {},
                .double_colon_token
                = cst::Token::from_lexical(context.require_extract(Token::Type::double_colon)),
                .source_range = global.source_range,
            });
    }

    auto extract_reference_dereference(Context& context, Token const& asterisk)
        -> cst::Expression::Variant
    {
        return cst::expression::Reference_dereference {
            .dereferenced_expression = require<parse_expression>(context, "an expression"),
            .asterisk_token          = cst::Token::from_lexical(asterisk),
        };
    }

    auto extract_tuple(Context& context, Token const& paren_open) -> cst::Expression::Variant
    {
        auto expressions
            = extract_comma_separated_zero_or_more<parse_expression, "an expression">(context);
        Token const paren_close = context.require_extract(Token::Type::paren_close);
        if (expressions.elements.size() == 1) {
            return cst::expression::Parenthesized { {
                .value       = expressions.elements.front(),
                .open_token  = cst::Token::from_lexical(paren_open),
                .close_token = cst::Token::from_lexical(paren_close),
            } };
        }
        return cst::expression::Tuple { {
            .value       = std::move(expressions),
            .open_token  = cst::Token::from_lexical(paren_open),
            .close_token = cst::Token::from_lexical(paren_close),
        } };
    }

    auto extract_array(Context& context, Token const& bracket_open) -> cst::Expression::Variant
    {
        auto elements
            = extract_comma_separated_zero_or_more<parse_expression, "an array element">(context);
        if (auto const bracket_close = context.try_extract(Token::Type::bracket_close)) {
            return cst::expression::Array_literal { {
                .value       = std::move(elements),
                .open_token  = cst::Token::from_lexical(bracket_open),
                .close_token = cst::Token::from_lexical(bracket_close.value()),
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

    auto extract_conditional(
        Context&               context,
        Token const&           if_or_elif_keyword,
        Conditional_kind const conditional_kind) -> cst::Expression::Variant
    {
        static constexpr auto extract_branch = [](Context& context) -> utl::wrapper auto {
            if (auto branch = parse_block_expression(context)) {
                return branch.value();
            }
            context.error_expected("a block expression");
        };

        // TODO: cleanup

        auto const primary_condition = extract_condition(context);
        auto const true_branch       = extract_branch(context);

        std::optional<cst::expression::Conditional::False_branch> false_branch;

        if (auto const elif_keyword = context.try_extract(Token::Type::elif)) {
            auto elif_conditional
                = extract_conditional(context, elif_keyword.value(), Conditional_kind::elif);

            false_branch = cst::expression::Conditional::False_branch {
                .body = context.wrap(cst::Expression {
                    .value        = std::move(elif_conditional),
                    .source_range = context.up_to_current(elif_keyword.value().source_range),
                }),

                .else_or_elif_keyword_token = cst::Token::from_lexical(elif_keyword.value()),
            };
        }
        else if (auto const else_keyword = context.try_extract(Token::Type::else_)) {
            false_branch = cst::expression::Conditional::False_branch {
                .body                       = extract_branch(context),
                .else_or_elif_keyword_token = cst::Token::from_lexical(else_keyword.value()),
            };
        }

        return cst::expression::Conditional {
            .condition                = primary_condition,
            .true_branch              = true_branch,
            .false_branch             = false_branch,
            .if_or_elif_keyword_token = cst::Token::from_lexical(if_or_elif_keyword),
            .is_elif_conditional      = conditional_kind == Conditional_kind::elif,
        };
    }

    auto extract_let_binding(Context& context, Token const& let_keyword) -> cst::Expression::Variant
    {
        auto const pattern     = extract_required<parse_top_level_pattern, "a pattern">(context);
        auto const type        = parse_type_annotation(context);
        auto const equals_sign = context.require_extract(Token::Type::equals);
        return cst::expression::Let_binding {
            .pattern           = pattern,
            .type              = type,
            .initializer       = require<parse_expression>(context, "the initializer expression"),
            .let_keyword_token = cst::Token::from_lexical(let_keyword),
            .equals_sign_token = cst::Token::from_lexical(equals_sign),
        };
    }

    auto extract_local_type_alias(Context& context, Token const& alias_keyword)
        -> cst::Expression::Variant
    {
        auto const name        = extract_upper_name(context, "an alias name");
        auto const equals_sign = context.require_extract(Token::Type::equals);
        return cst::expression::Local_type_alias {
            .alias_name          = name,
            .aliased_type        = require<parse_type>(context, "an aliased type"),
            .alias_keyword_token = cst::Token::from_lexical(alias_keyword),
            .equals_sign_token   = cst::Token::from_lexical(equals_sign),
        };
    }

    auto extract_sizeof(Context& context, Token const& sizeof_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Sizeof {
            .inspected_type
            = require<parse_parenthesized<parse_type, "a type">>(context, "a parenthesized type"),
            .sizeof_keyword_token = cst::Token::from_lexical(sizeof_keyword),
        };
    }

    auto extract_addressof(Context& context, Token const& addressof_keyword)
        -> cst::Expression::Variant
    {
        return cst::expression::Addressof {
            .lvalue_expression
            = require<parse_parenthesized<parse_expression, "an addressable expression">>(
                context, "a parenthesized addressable expression"),
            .addressof_keyword_token = cst::Token::from_lexical(addressof_keyword),
        };
    }

    auto extract_pointer_dereference(Context& context, Token const& dereference_keyword)
        -> cst::Expression::Variant
    {
        return cst::expression::Pointer_dereference {
            .pointer_expression
            = require<parse_parenthesized<parse_expression, "a pointer expression">>(
                context, "a parenthesized pointer expression"),
            .dereference_keyword_token = cst::Token::from_lexical(dereference_keyword),
        };
    }

    auto parse_match_case(Context& context) -> std::optional<cst::expression::Match::Case>
    {
        return parse_top_level_pattern(context).transform(
            [&](utl::Wrapper<cst::Pattern> const pattern) {
                auto const arrow     = context.require_extract(Token::Type::right_arrow);
                auto const handler   = require<parse_expression>(context, "an expression");
                auto const semicolon = context.try_extract(Token::Type::semicolon);
                return cst::expression::Match::Case {
                    .pattern                  = pattern,
                    .handler                  = handler,
                    .arrow_token              = cst::Token::from_lexical(arrow),
                    .optional_semicolon_token = semicolon.transform(cst::Token::from_lexical),
                };
            });
    }

    auto parse_match_cases(Context& context)
        -> std::optional<std::vector<cst::expression::Match::Case>>
    {
        std::vector<cst::expression::Match::Case> cases;
        while (auto match_case = parse_match_case(context)) {
            cases.push_back(std::move(match_case.value()));
        }
        return !cases.empty() ? std::optional(std::move(cases)) : std::nullopt;
    }

    auto extract_match(Context& context, Token const& match_keyword) -> cst::Expression::Variant
    {
        static constexpr auto extract_cases = extract_required<
            parse_braced<parse_match_cases, "one or more match cases">,
            "a '{' followed by match cases">;
        auto const expression = require<parse_expression>(context, "an expression");
        return cst::expression::Match {
            .cases               = extract_cases(context),
            .matched_expression  = expression,
            .match_keyword_token = cst::Token::from_lexical(match_keyword),
        };
    }

    auto extract_continue(Context&, Token const& continue_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Continue {
            .continue_keyword_token = cst::Token::from_lexical(continue_keyword),
        };
    }

    auto extract_break(Context& context, Token const& break_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Break {
            .result              = parse_expression(context),
            .break_keyword_token = cst::Token::from_lexical(break_keyword),
        };
    }

    auto extract_ret(Context& context, Token const& ret_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Ret {
            .returned_expression = parse_expression(context),
            .ret_keyword_token   = cst::Token::from_lexical(ret_keyword),
        };
    }

    auto extract_discard(Context& context, Token const& discard_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Discard {
            .discarded_expression  = require<parse_expression>(context, "the discarded expression"),
            .discard_keyword_token = cst::Token::from_lexical(discard_keyword),
        };
    }

    auto extract_reference(Context& context, Token const& ampersand) -> cst::Expression::Variant
    {
        return cst::expression::Reference {
            .mutability = parse_mutability(context),
            .referenced_expression
            = require<parse_expression>(context, "the referenced expression"),
            .ampersand_token = cst::Token::from_lexical(ampersand),
        };
    }

    auto extract_unsafe_block(Context& context, Token const& unsafe_keyword)
        -> cst::Expression::Variant
    {
        return cst::expression::Unsafe {
            .expression = require<parse_block_expression>(context, "an unsafe block expression"),
            .unsafe_keyword_token = cst::Token::from_lexical(unsafe_keyword),
        };
    }

    auto extract_move(Context& context, Token const& mov_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Move {
            .lvalue            = require<parse_expression>(context, "an expression"),
            .mov_keyword_token = cst::Token::from_lexical(mov_keyword),
        };
    }

    auto extract_meta(Context& context, Token const& meta_keyword) -> cst::Expression::Variant
    {
        return cst::expression::Meta {
            .expression = require<parse_parenthesized<parse_expression, "an expression">>(
                context, "a parenthesized expression"),
            .meta_keyword_token = cst::Token::from_lexical(meta_keyword),
        };
    }

    auto extract_block_expression(Context& context, Token const& brace_open)
        -> cst::Expression::Variant
    {
        std::vector<cst::expression::Block::Side_effect> side_effects;
        std::optional<utl::Wrapper<cst::Expression>>     result_expression;

        while (auto expression = parse_expression(context)) {
            if (auto const semicolon = context.try_extract(Token::Type::semicolon)) {
                side_effects.push_back(cst::expression::Block::Side_effect {
                    .expression               = expression.value(),
                    .trailing_semicolon_token = cst::Token::from_lexical(semicolon.value()),
                });
            }
            else {
                result_expression = expression.value();
                break;
            }
        }

        Token const brace_close = context.require_extract(Token::Type::brace_close);
        return cst::expression::Block {
            .side_effects      = std::move(side_effects),
            .result_expression = std::move(result_expression),
            .open_brace_token  = cst::Token::from_lexical(brace_open),
            .close_brace_token = cst::Token::from_lexical(brace_close),
        };
    }

    auto parse_complicated_type(Context& context, Stage const stage)
        -> std::optional<cst::Expression::Variant>
    {
        context.unstage(stage);
        return parse_type(context).transform(
            [&](utl::Wrapper<cst::Type> const type) -> cst::Expression::Variant {
                if (auto const double_colon = context.try_extract(Token::Type::double_colon)) {
                    return extract_qualified_lower_name_or_struct_initializer(
                        context,
                        cst::Root_qualifier {
                            .value              = type,
                            .double_colon_token = cst::Token::from_lexical(double_colon.value()),
                            .source_range       = type->source_range,
                        });
                }

                if (auto const brace_open = context.try_extract(Token::Type::brace_open)) {
                    return extract_struct_initializer(context, brace_open.value(), type);
                }
                context.compile_info().diagnostics.error(
                    context.source(),
                    type->source_range,
                    "Expected an expression, but found a type");
            });
    }

    auto dispatch_parse_normal_expression(Context& context, Token const& token, Stage const stage)
        -> std::optional<cst::Expression::Variant>
    {
        // clang-format off
        switch (token.type) {
        case Token::Type::integer_literal:   return token.value_as<kieli::Integer>();
        case Token::Type::floating_literal:  return token.value_as<kieli::Floating>();
        case Token::Type::character_literal: return token.value_as<kieli::Character>();
        case Token::Type::boolean_literal:   return token.value_as<kieli::Boolean>();
        case Token::Type::string_literal:    return extract_string_literal(context, token);
        case Token::Type::lower_name:        [[fallthrough]];
        case Token::Type::upper_name:        return extract_identifier(context, stage);
        case Token::Type::global:            return extract_global_identifier(context, token);
        case Token::Type::lower_self:        return cst::expression::Self {};
        case Token::Type::hole:              return cst::expression::Hole {};
        case Token::Type::asterisk:          return extract_reference_dereference(context, token);
        case Token::Type::paren_open:        return extract_tuple(context, token);
        case Token::Type::bracket_open:      return extract_array(context, token);
        case Token::Type::if_:               return extract_conditional(context, token, Conditional_kind::if_);
        case Token::Type::let:               return extract_let_binding(context, token);
        case Token::Type::alias:             return extract_local_type_alias(context, token);
        case Token::Type::loop:              return extract_infinite_loop(context, token);
        case Token::Type::while_:            return extract_while_loop(context, token);
        case Token::Type::for_:              return extract_for_loop(context, token);
        case Token::Type::sizeof_:           return extract_sizeof(context, token);
        case Token::Type::addressof:         return extract_addressof(context, token);
        case Token::Type::dereference:       return extract_pointer_dereference(context, token);
        case Token::Type::unsafe:            return extract_unsafe_block(context, token);
        case Token::Type::match:             return extract_match(context, token);
        case Token::Type::continue_:         return extract_continue(context, token);
        case Token::Type::break_:            return extract_break(context, token);
        case Token::Type::ret:               return extract_ret(context, token);
        case Token::Type::discard:           return extract_discard(context, token);
        case Token::Type::ampersand:         return extract_reference(context, token);
        case Token::Type::mov:               return extract_move(context, token);
        case Token::Type::meta:              return extract_meta(context, token);
        case Token::Type::brace_open:        return extract_block_expression(context, token);
        default:                             return parse_complicated_type(context, stage);
            // clang-format on
        }
    }

    auto parse_normal_expression(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
    {
        Stage const stage       = context.stage();
        Token const first_token = context.extract();
        if (auto variant = dispatch_parse_normal_expression(context, first_token, stage)) {
            return context.wrap(cst::Expression {
                .value        = std::move(variant.value()),
                .source_range = context.up_to_current(first_token.source_range),
            });
        }
        context.unstage(stage);
        return std::nullopt;
    }

    auto parse_potential_invocation(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_normal_expression(context).transform(
            [&](utl::Wrapper<cst::Expression> expression) {
                while (auto arguments = parse_function_arguments(context)) {
                    expression = context.wrap(cst::Expression {
                        .value = cst::expression::Invocation {
                            .function_arguments  = std::move(arguments.value()),
                            .function_expression = expression,
                        },
                        .source_range = context.up_to_current(expression->source_range),
                    });
                }
                return expression;
            });
    }

    auto extract_struct_field_access(
        kieli::Name_lower const             field_name,
        cst::Token const                    dot_token,
        utl::Wrapper<cst::Expression> const expression,
        Context&                            context) -> cst::Expression::Variant
    {
        auto template_arguments = parse_template_arguments(context);
        if (auto arguments = parse_function_arguments(context)) {
            return cst::expression::Method_invocation {
                .function_arguments = std::move(arguments.value()),
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
            .dot_token       = dot_token,
        };
    }

    auto extract_member_access(
        cst::Token const                    dot_token,
        utl::Wrapper<cst::Expression> const base_expression,
        Context&                            context) -> cst::Expression::Variant
    {
        if (auto const name = parse_lower_name(context)) {
            return extract_struct_field_access(name.value(), dot_token, base_expression, context);
        }
        if (auto const index = parse_bracketed<parse_expression, "an index expression">(context)) {
            return cst::expression::Array_index_access {
                .base_expression  = base_expression,
                .index_expression = index.value(),
                .dot_token        = dot_token,
            };
        }
        if (auto const literal = context.try_extract(Token::Type::integer_literal)) {
            return cst::expression::Tuple_field_access {
                .base_expression   = base_expression,
                .field_index       = literal.value().value_as<kieli::Integer>().value,
                .field_index_token = cst::Token::from_lexical(literal.value()),
                .dot_token         = dot_token,
            };
        }
        context.error_expected(
            "a struct member name (a.b), a tuple member index (a.0), or an array index (a.[b])");
    }

    auto parse_potential_member_access(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_potential_invocation(context).transform(
            [&](utl::Wrapper<cst::Expression> expression) {
                while (auto const dot = context.try_extract(Token::Type::dot)) {
                    expression = context.wrap(cst::Expression {
                        .value = extract_member_access(
                            cst::Token::from_lexical(dot.value()), expression, context),
                        .source_range = context.up_to_current(expression->source_range),
                    });
                }
                return expression;
            });
    }

    auto dispatch_parse_type_cast(
        Context& context, utl::Wrapper<cst::Expression> const base_expression)
        -> std::optional<cst::Expression::Variant>
    {
        switch (context.peek().type) {
        case Token::Type::colon:
            return cst::expression::Type_ascription {
                .base_expression = base_expression,
                .colon_token     = cst::Token::from_lexical(context.extract()),
                .ascribed_type   = require<parse_type>(context, "the ascribed type"),
            };
        case Token::Type::as:
            return cst::expression::Type_cast {
                .base_expression = base_expression,
                .as_token        = cst::Token::from_lexical(context.extract()),
                .target_type     = require<parse_type>(context, "the target type"),
            };
        default:
            return std::nullopt;
        }
    }

    auto parse_potential_type_cast(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_potential_member_access(context).transform(
            [&](utl::Wrapper<cst::Expression> expression) {
                while (auto type_cast = dispatch_parse_type_cast(context, expression)) {
                    expression = context.wrap(cst::Expression {
                        .value        = std::move(type_cast.value()),
                        .source_range = context.up_to_current(expression->source_range),
                    });
                }
                return expression;
            });
    }

    auto parse_operator_id(Context& context) -> std::optional<kieli::Identifier>
    {
        Stage const stage = context.stage();
        Token const token = context.extract();
        switch (token.type) {
        case Token::Type::operator_name:
            return token.value_as<kieli::Identifier>();
        case Token::Type::asterisk:
            return context.special_identifiers().asterisk;
        case Token::Type::plus:
            return context.special_identifiers().plus;
        default:
            context.unstage(stage);
            return std::nullopt;
        }
    }

    auto parse_operator_name(Context& context)
        -> std::optional<cst::expression::Binary_operator_invocation_sequence::Operator_name>
    {
        auto const anchor_source_range = context.peek().source_range;
        return parse_operator_id(context).transform([&](kieli::Identifier const identifier) {
            return cst::expression::Binary_operator_invocation_sequence::Operator_name {
                .operator_id  = identifier,
                .source_range = anchor_source_range,
            };
        });
    }

    auto parse_binary_operator_invocation_sequence(Context& context)
        -> std::optional<utl::Wrapper<cst::Expression>>
    {
        return parse_potential_type_cast(context).transform([&](utl::wrapper auto expression) {
            std::vector<cst::expression::Binary_operator_invocation_sequence::Operator_and_operand>
                tail;
            while (auto const operator_name = parse_operator_name(context)) {
                tail.push_back({
                    .right_operand = require<parse_potential_type_cast>(context, "an operand"),
                    .operator_name = operator_name.value(),
                });
            }
            if (tail.empty()) {
                return expression;
            }
            return context.wrap(cst::Expression {
                .value { cst::expression::Binary_operator_invocation_sequence {
                    .sequence_tail    = std::move(tail),
                    .leftmost_operand = expression,
                } },
                .source_range = context.up_to_current(expression->source_range),
            });
        });
    }

} // namespace

auto libparse::parse_block_expression(Context& context)
    -> std::optional<utl::Wrapper<cst::Expression>>
{
    return context.peek().type == Token::Type::brace_open ? parse_normal_expression(context)
                                                          : std::nullopt;
}

auto libparse::parse_expression(Context& context) -> std::optional<utl::Wrapper<cst::Expression>>
{
    return parse_binary_operator_invocation_sequence(context);
}
