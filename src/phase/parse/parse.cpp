#include "utl/utilities.hpp"
#include "parser_internals.hpp"
#include "parse.hpp"


namespace {

    auto parse_definition(Parse_context&) -> tl::optional<ast::Definition>;


    template <class Definition>
    auto definition(tl::optional<std::vector<ast::Template_parameter>>&& parameters, Definition&& definition)
        -> ast::Definition::Variant
    {
        if (parameters.has_value()) {
            return ast::definition::Template<std::decay_t<Definition>> {
                .definition = std::forward<Definition>(definition),
                .parameters = std::move(*parameters)
            };
        }
        return std::forward<Definition>(definition);
    }


    auto extract_definition_sequence(Parse_context& context) -> std::vector<ast::Definition> {
        std::vector<ast::Definition> definitions = utl::vector_with_capacity(16);
        while (auto definition = parse_definition(context))
            definitions.push_back(std::move(*definition));
        return definitions;
    }


    auto extract_braced_definition_sequence(Parse_context& context) -> std::vector<ast::Definition> {
        context.consume_required(Token::Type::brace_open);
        auto definitions = extract_definition_sequence(context);
        context.consume_required(Token::Type::brace_close);
        return definitions;
    }


    auto parse_self_parameter(Parse_context& context) -> tl::optional<ast::Self_parameter> {
        Token* const anchor = context.pointer;
        using Self = ast::Self_parameter;

        ast::Mutability mutability = extract_mutability(context);
        if (Token* const self = context.try_extract(Token::Type::lower_self)) {
            return Self {
                .mutability   = std::move(mutability),
                .is_reference = false,
                .source_view  = self->source_view
            };
        }
        else if (context.try_consume(Token::Type::ampersand)) {
            if (mutability.was_explicitly_specified())
                context.error(mutability.source_view, { "A mutability specifier can not appear here" });

            ast::Mutability reference_mutability = extract_mutability(context);
            if (Token* const self = context.try_extract(Token::Type::lower_self)) {
                return Self {
                    .mutability   = std::move(reference_mutability),
                    .is_reference = true,
                    .source_view  = self->source_view
                };
            }
        }

        context.pointer = anchor;
        return tl::nullopt;
    }


    auto extract_function_signature(Parse_context& context)
        -> utl::Pair<ast::Function_signature, tl::optional<std::vector<ast::Template_parameter>>>
    {
        auto name = extract_lower_name(context, "a function name");
        auto template_parameters = parse_template_parameters(context);

        if (!context.try_consume(Token::Type::paren_open))
            context.error_expected("a parenthesized list of function parameters");

        auto self_parameter = parse_self_parameter(context);

        std::vector<ast::Function_parameter> parameters;
        if (!self_parameter.has_value() || context.try_consume(Token::Type::comma))
            parameters = extract_function_parameters(context);

        context.consume_required(Token::Type::paren_close);

        tl::optional<ast::Type> return_type;
        if (context.try_consume(Token::Type::colon))
            return_type = extract_type(context);

        if (context.try_consume(Token::Type::where))
            utl::todo(); // TODO: add support for where clauses

        return {
            ast::Function_signature {
                .parameters     = std::move(parameters),
                .self_parameter = std::move(self_parameter),
                .return_type    = return_type,
                .name           = std::move(name),
            },
            std::move(template_parameters),
        };
    }

    auto extract_function(Parse_context& context)
        -> ast::Definition::Variant
    {
        auto [signature, template_parameters] = extract_function_signature(context);

        auto body = std::invoke([&] {
            if (auto expression = parse_block_expression(context))
                return std::move(*expression);
            else if (context.try_consume(Token::Type::equals))
                return extract_expression(context);
            else
                context.error_expected("the function body", "'=' or '{{'");
        });

        return definition(
            std::move(template_parameters),
            ast::definition::Function {
                .signature = std::move(signature),
                .body      = std::move(body),
            });
    };


    template <class Member>
    auto ensure_no_duplicate_members(
        Parse_context            & context,
        std::vector<Member> const& range,
        std::string_view    const  description) -> void
    {
        for (auto it = range.cbegin(); it != range.cend(); ++it) {
            auto const found = ranges::find(range.cbegin(), it, it->name, &Member::name);
            if (found == it) continue;

            context.compilation_info.get()->diagnostics.emit_error({
                .sections = utl::to_vector({
                    utl::diagnostics::Text_section {
                        .source_view = found->source_view,
                        .note        = "Previously defined here",
                        .note_color  = utl::diagnostics::warning_color,
                    },
                    utl::diagnostics::Text_section {
                        .source_view = it->source_view,
                        .note        = "Later defined here",
                        .note_color  = utl::diagnostics::error_color,
                    },
                }),
                .message           = "Duplicate definition of {} {}",
                .message_arguments = fmt::make_format_args(description, it->name),
            });
        }
    }


    auto parse_struct_member(Parse_context& context)
        -> tl::optional<ast::definition::Struct::Member>
    {
        auto* const anchor    = context.pointer;
        bool  const is_public = context.try_consume(Token::Type::pub);

        if (auto name = parse_lower_name(context)) {
            context.consume_required(Token::Type::colon);

            auto type = extract_type(context);

            return ast::definition::Struct::Member {
                .name        = *name,
                .type        = std::move(type),
                .is_public   = is_public,
                .source_view = make_source_view(anchor, context.pointer - 1)
            };
        }
        if (is_public)
            context.error_expected("a struct member name");
        else
            return tl::nullopt;
    }

    auto extract_struct(Parse_context& context)
        -> ast::Definition::Variant
    {
        static constexpr auto parse_members =
            parse_comma_separated_one_or_more<parse_struct_member, "a struct member">;

        auto name                = extract_upper_name(context, "a struct name");
        auto template_parameters = parse_template_parameters(context);

        context.consume_required(Token::Type::equals);

        if (auto members = parse_members(context)) {
            ensure_no_duplicate_members(context, *members, "member");

            return definition(
                std::move(template_parameters),
                ast::definition::Struct {
                    std::move(*members),
                    std::move(name)
                });
        }
        context.error_expected("one or more struct members");
    };


    auto parse_enum_constructor(Parse_context& context)
        -> tl::optional<ast::definition::Enum::Constructor>
    {
        auto* const anchor = context.pointer;

        if (auto name = parse_lower_name(context)) {
            tl::optional<ast::Type> payload_type;

            if (context.try_consume(Token::Type::paren_open)) {
                auto types = extract_type_sequence(context);

                switch (types.size()) {
                case 0:
                    break;
                case 1:
                    payload_type = std::move(types.front());
                    break;
                default:
                {
                    utl::Source_view const view = types.front().source_view + types.back().source_view;
                    payload_type = ast::Type {
                        .value       = ast::type::Tuple { std::move(types) },
                        .source_view = view
                    };
                }
                }

                context.consume_required(Token::Type::paren_close);
            }

            return ast::definition::Enum::Constructor {
                .name         = *name,
                .payload_type = std::move(payload_type),
                .source_view  = make_source_view(anchor, context.pointer - 1)
            };
        }
        return tl::nullopt;
    }

    auto extract_enum(Parse_context& context)
        -> ast::Definition::Variant
    {
        static constexpr auto parse_constructors =
            parse_separated_one_or_more<parse_enum_constructor, Token::Type::pipe, "an enum constructor">;

        auto* anchor = context.pointer;

        auto name                = extract_upper_name(context, "an enum name");
        auto template_parameters = parse_template_parameters(context);

        context.consume_required(Token::Type::equals);
        if (auto constructors = parse_constructors(context)) {
            ensure_no_duplicate_members(context, *constructors, "constructor");

            static constexpr auto max =
                std::numeric_limits<utl::U8>::max();

            if (constructors->size() > max) {
                // This allows the tag to always be a single byte
                context.error(
                    { anchor - 1, anchor + 1 },
                    {
                        .message =
                            "An enum-definition must not define more "
                            "than {} constructors, but {} defines {}",
                        .message_arguments = fmt::make_format_args(max, name, constructors->size()),
                        .help_note =
                            "If this is truly necessary, consider categorizing "
                            "the constructors under several simpler types"
                    }
                );
            }

            return definition(
                std::move(template_parameters),
                ast::definition::Enum {
                    std::move(*constructors),
                    std::move(name)
                });
        }
        context.error_expected("one or more enum constructors");
    };


    auto extract_alias(Parse_context& context)
        -> ast::Definition::Variant
    {
        auto name                = extract_upper_name(context, "an alias name");
        auto template_parameters = parse_template_parameters(context);

        context.consume_required(Token::Type::equals);

        return definition(
            std::move(template_parameters),
            ast::definition::Alias {
                name,
                extract_type(context)
            });
    };


    auto extract_implementation(Parse_context& context)
        -> ast::Definition::Variant
    {
        auto template_parameters = parse_template_parameters(context);
        auto type                = extract_type(context);
        auto definitions         = extract_braced_definition_sequence(context);

        return definition(
            std::move(template_parameters),
            ast::definition::Implementation {
                std::move(type),
                std::move(definitions)
            });
    };


    auto extract_instantiation(Parse_context& context)
        -> ast::Definition::Variant
    {
        auto template_parameters = parse_template_parameters(context);

        if (auto typeclass = parse_class_reference(context)) {
            context.consume_required(Token::Type::for_);

            auto type        = extract_type(context);
            auto definitions = extract_braced_definition_sequence(context);

            return definition(
                std::move(template_parameters),
                ast::definition::Instantiation {
                    .typeclass   = std::move(*typeclass),
                    .self_type   = std::move(type),
                    .definitions = std::move(definitions)
                });
        }
        context.error_expected("a class name");
    };


    auto extract_function_signature(
        Parse_context                                             & context,
        std::output_iterator<ast::Function_template_signature> auto template_out,
        std::output_iterator<ast::Function_signature>          auto nontemplate_out) -> void
    {
        auto [signature, template_parameters] = extract_function_signature(context);

        if (!signature.return_type.has_value())
            context.error(signature.name.source_view, { "Function signature return type missing" });

        if (template_parameters.has_value()) {
            *template_out = ast::Function_template_signature {
                .function_signature  = std::move(signature),
                .template_parameters = std::move(*template_parameters)
            };
        }
        else {
            *nontemplate_out = std::move(signature);
        }
    }

    auto extract_type_signature(
        Parse_context                                         & context,
        std::output_iterator<ast::Type_template_signature> auto template_out,
        std::output_iterator<ast::Type_signature>          auto nontemplate_out) -> void
    {
        auto name                = extract_upper_name(context, "an alias name");
        auto template_parameters = parse_template_parameters(context);

        std::vector<ast::Class_reference> classes;
        if (context.try_consume(Token::Type::colon))
            classes = extract_class_references(context);

        ast::Type_signature signature {
            .classes = std::move(classes),
            .name    = std::move(name)
        };

        if (template_parameters.has_value()) {
            *template_out = ast::Type_template_signature {
                .type_signature      = std::move(signature),
                .template_parameters = std::move(*template_parameters)
            };
        }
        else {
            *nontemplate_out = std::move(signature);
        }
    }

    auto extract_class(Parse_context& context)
        -> ast::Definition::Variant
    {
        auto name                = extract_upper_name(context, "a class name");
        auto template_parameters = parse_template_parameters(context);

        std::vector<ast::Type_signature>              type_signatures;
        std::vector<ast::Type_template_signature>     type_template_signatures;
        std::vector<ast::Function_signature>          function_signatures;
        std::vector<ast::Function_template_signature> function_template_signatures;

        bool const is_braced = context.try_consume(Token::Type::brace_open);
        if (!is_braced)
            context.consume_required(Token::Type::equals);

        for (;;) {
            switch (context.extract().type) {
            case Token::Type::fn:
                extract_function_signature(
                    context,
                    std::back_inserter(function_template_signatures),
                    std::back_inserter(function_signatures));
                continue;
            case Token::Type::alias:
                extract_type_signature(
                    context,
                    std::back_inserter(type_template_signatures),
                    std::back_inserter(type_signatures));
                continue;
            default:
                context.retreat();

                if (is_braced)
                    context.consume_required(Token::Type::brace_close);

                return definition(
                    std::move(template_parameters),
                    ast::definition::Typeclass {
                        .function_signatures          = std::move(function_signatures),
                        .function_template_signatures = std::move(function_template_signatures),
                        .type_signatures              = std::move(type_signatures),
                        .type_template_signatures     = std::move(type_template_signatures),
                        .name                         = std::move(name)
                    });
            }
        }
    };


    auto extract_namespace(Parse_context& context)
        -> ast::Definition::Variant
    {
        auto name                = extract_lower_name(context, "a namespace name");
        auto template_parameters = parse_template_parameters(context);

        utl::always_assert(!template_parameters.has_value());

        return ast::definition::Namespace {
            extract_braced_definition_sequence(context),
            std::move(name)
        };
    };


    auto parse_definition(Parse_context& context)
        -> tl::optional<ast::Definition>
    {
        return parse_node<ast::Definition, [](Parse_context& context)
            -> tl::optional<ast::Definition::Variant>
        {
            switch (context.extract().type) {
            case Token::Type::fn:
                return extract_function(context);
            case Token::Type::struct_:
                return extract_struct(context);
            case Token::Type::enum_:
                return extract_enum(context);
            case Token::Type::alias:
                return extract_alias(context);
            case Token::Type::class_:
                return extract_class(context);
            case Token::Type::impl:
                return extract_implementation(context);
            case Token::Type::inst:
                return extract_instantiation(context);
            case Token::Type::namespace_:
                return extract_namespace(context);
            default:
                context.retreat();
                return tl::nullopt;
            }
        }>(context);
    }


    auto validate_module_name_or_path(Parse_context& context, Token const& token) -> compiler::String {
        compiler::String const module_string = token.as_string();
        if (module_string.view().find(".."sv) != std::string_view::npos)
            context.error(token.source_view, { "A module name or path can not contain '..'" });
        return module_string;
    }

}


auto compiler::parse(Lex_result&& lex_result) -> Parse_result {
    Parse_context context { std::move(lex_result), ast::Node_arena::with_default_page_size() };

    std::vector<compiler::String>  module_imports;
    tl::optional<compiler::String> module_name;

    if (context.try_consume(Token::Type::module_)) {
        if (Token const* const name = context.try_extract(Token::Type::string))
            module_name = validate_module_name_or_path(context, *name);
        else
            context.error_expected("a module name");
    }

    while (context.try_consume(Token::Type::import_)) {
        if (Token const* const name = context.try_extract(Token::Type::string))
            module_imports.push_back(validate_module_name_or_path(context, *name));
        else
            context.error_expected("a module path");
    }

    auto definitions = extract_definition_sequence(context);

    if (!context.is_finished()) {
        context.error_expected("fn, struct, enum, alias, class, impl, inst, or namespace");
    }

    return Parse_result {
        .compilation_info = std::move(context.compilation_info),
        .node_arena       = std::move(context.node_arena),
        .module {
            .definitions = std::move(definitions),
            .name        = std::move(module_name),
            .imports     = std::move(module_imports),
        }
    };
}
