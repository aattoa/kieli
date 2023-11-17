#include <libparse/parse.hpp>
#include <libparse/parser_internals.hpp>
#include <libutl/common/utilities.hpp>

namespace {

    using namespace libparse;

    auto parse_definition(Parse_context&) -> std::optional<cst::Definition>;

    auto extract_definition_sequence(Parse_context& context) -> std::vector<cst::Definition>
    {
        std::vector<cst::Definition> definitions = utl::vector_with_capacity(16);
        while (auto definition = parse_definition(context)) {
            definitions.push_back(std::move(*definition)); // NOLINT: false positive
        }
        return definitions;
    }

    auto extract_braced_definition_sequence(Parse_context& context) -> std::vector<cst::Definition>
    {
        context.consume_required(Token_type::brace_open);
        auto definitions = extract_definition_sequence(context);
        context.consume_required(Token_type::brace_close);
        return definitions;
    }

    auto parse_self_parameter(Parse_context& context) -> std::optional<cst::Self_parameter>
    {
        Lexical_token* const anchor = context.pointer;

        std::optional mutability = parse_mutability(context);
        if (Lexical_token const* const self = context.try_extract(Token_type::lower_self)) {
            return cst::Self_parameter {
                .mutability         = std::move(mutability),
                .self_keyword_token = cst::Token::from_lexical(self),
                .source_view        = self->source_view,
            };
        }

        if (Lexical_token const* const ampersand = context.try_extract(Token_type::ampersand)) {
            if (mutability.has_value()) {
                context.diagnostics().error(
                    mutability->source_view, "A mutability specifier can not appear here");
            }
            std::optional reference_mutability = parse_mutability(context);
            if (Lexical_token const* const self = context.try_extract(Token_type::lower_self)) {
                return cst::Self_parameter {
                    .mutability         = std::move(reference_mutability),
                    .ampersand_token    = cst::Token::from_lexical(ampersand),
                    .self_keyword_token = cst::Token::from_lexical(self),
                    .source_view        = self->source_view,
                };
            }
        }

        context.pointer = anchor;
        return std::nullopt;
    }

    auto parse_function_parameters(Parse_context& context)
        -> std::optional<cst::Function_parameters>
    {
        Lexical_token const* const open = context.try_extract(Token_type::paren_open);
        if (!open) {
            return std::nullopt;
        }

        auto                      self_parameter = parse_self_parameter(context);
        std::optional<cst::Token> comma_token_after_self;
        cst::Separated_sequence<cst::Function_parameter> normal_parameters;

        if (self_parameter.has_value()) {
            comma_token_after_self = optional_token(context.try_extract(Token_type::comma));
            if (comma_token_after_self.has_value()) {
                normal_parameters = extract_function_parameters(context);
            }
        }
        else {
            normal_parameters = extract_function_parameters(context);
        }

        Lexical_token const* const close = context.extract_required(Token_type::paren_close);
        return cst::Function_parameters {
            .normal_parameters                = std::move(normal_parameters),
            .self_parameter                   = std::move(self_parameter),
            .comma_token_after_self_parameter = comma_token_after_self,
            .open_parenthesis_token           = cst::Token::from_lexical(open),
            .close_parenthesis_token          = cst::Token::from_lexical(close),
        };
    }

    auto extract_function_signature(Parse_context& context) -> cst::Function_signature
    {
        Lexical_token const* const fn_keyword = context.pointer - 1;

        auto name                = extract_lower_name(context, "a function name");
        auto template_parameters = parse_template_parameters(context);
        auto function_parameters = parse_function_parameters(context);

        if (!function_parameters.has_value()) {
            context.error_expected("a '(' followed by a function parameter list");
        }

        std::optional return_type_annotation = parse_type_annotation(context);

        if (context.try_consume(Token_type::where)) {
            utl::todo(); // TODO: add support for where clauses
        }

        return cst::Function_signature {
            .template_parameters = std::move(template_parameters),
            .function_parameters = std::move(*function_parameters),
            .return_type         = std::move(return_type_annotation),
            .name                = std::move(name),
            .fn_keyword_token    = cst::Token::from_lexical(fn_keyword),
        };
    }

    auto extract_function(Parse_context& context) -> cst::Definition::Variant
    {
        Lexical_token const* const fn_keyword  = context.pointer - 1;
        Lexical_token const*       equals_sign = nullptr;
        auto                       signature   = extract_function_signature(context);

        utl::wrapper auto const body = std::invoke([&] {
            if (auto expression = parse_block_expression(context)) {
                return *expression;
            }
            equals_sign = context.try_extract(Token_type::equals);
            if (equals_sign) {
                return extract_expression(context);
            }
            context.error_expected("the function body: '=' or '{'");
        });

        return cst::definition::Function {
            .signature                  = std::move(signature),
            .body                       = body,
            .optional_equals_sign_token = optional_token(equals_sign),
            .fn_keyword_token           = cst::Token::from_lexical(fn_keyword),
        };
    };

    auto parse_struct_member(Parse_context& context)
        -> std::optional<cst::definition::Struct::Member>
    {
        Lexical_token const* const anchor    = context.pointer;
        bool const                 is_public = context.try_consume(Token_type::pub);

        if (auto name = parse_lower_name(context)) {
            if (auto type = parse_type_annotation(context)) {
                return cst::definition::Struct::Member {
                    .name        = *name,
                    .type        = *type,
                    .is_public   = is_public,
                    .source_view = context.make_source_view(anchor, context.pointer - 1),
                };
            }
            context.error_expected("a ':' followed by a type");
        }
        if (is_public) {
            context.error_expected("a struct member name");
        }
        else {
            return std::nullopt;
        }
    }

    auto extract_struct(Parse_context& context) -> cst::Definition::Variant
    {
        static constexpr auto parse_members
            = parse_comma_separated_one_or_more<parse_struct_member, "a struct member">;

        Lexical_token const* const struct_keyword = context.pointer - 1;

        auto name                = extract_upper_name(context, "a struct name");
        auto template_parameters = parse_template_parameters(context);

        Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);

        if (auto members = parse_members(context)) {
            return cst::definition::Struct {
                .template_parameters  = std::move(template_parameters),
                .members              = std::move(*members),
                .name                 = std::move(name),
                .struct_keyword_token = cst::Token::from_lexical(struct_keyword),
                .equals_sign_token    = cst::Token::from_lexical(equals_sign),
            };
        }
        context.error_expected("one or more struct members");
    };

    auto parse_enum_constructor(Parse_context& context)
        -> std::optional<cst::definition::Enum::Constructor>
    {
        Lexical_token const* const anchor = context.pointer;
        if (auto name = parse_lower_name(context)) {
            using Payload_types = decltype(cst::definition::Enum::Constructor::payload_types);
            Payload_types payload_types;
            if (Lexical_token const* const open = context.try_extract(Token_type::paren_open)) {
                auto types = parse_comma_separated_one_or_more<parse_type, "a type">(context);
                if (!types.has_value()) {
                    context.error_expected("one or more types");
                }
                Lexical_token const* const close
                    = context.extract_required(Token_type::paren_close);
                payload_types = Payload_types::value_type {
                    .value       = std::move(*types),
                    .open_token  = cst::Token::from_lexical(open),
                    .close_token = cst::Token::from_lexical(close),
                };
            }
            return cst::definition::Enum::Constructor {
                .payload_types = std::move(payload_types),
                .name          = *name,
                .source_view   = context.make_source_view(anchor, context.pointer - 1),
            };
        }
        return std::nullopt;
    }

    auto extract_enum(Parse_context& context) -> cst::Definition::Variant
    {
        static constexpr auto parse_constructors = parse_separated_one_or_more<
            parse_enum_constructor,
            Lexical_token::Type::pipe,
            "an enum constructor">;

        Lexical_token const* const enum_keyword = context.pointer - 1;

        auto name                = extract_upper_name(context, "an enum name");
        auto template_parameters = parse_template_parameters(context);

        Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);
        if (auto constructors = parse_constructors(context)) {
            return cst::definition::Enum {
                .template_parameters = std::move(template_parameters),
                .constructors        = std::move(*constructors),
                .name                = std::move(name),
                .enum_keyword_token  = cst::Token::from_lexical(enum_keyword),
                .equals_sign_token   = cst::Token::from_lexical(equals_sign),
            };
        }
        context.error_expected("one or more enum constructors");
    };

    auto extract_alias(Parse_context& context) -> cst::Definition::Variant
    {
        Lexical_token const* const alias_keyword = context.pointer - 1;

        auto name                = extract_upper_name(context, "an alias name");
        auto template_parameters = parse_template_parameters(context);

        Lexical_token const* const equals_sign = context.extract_required(Token_type::equals);
        return cst::definition::Alias {
            .template_parameters = std::move(template_parameters),
            .name                = std::move(name),
            .type                = extract_type(context),
            .alias_keyword_token = cst::Token::from_lexical(alias_keyword),
            .equals_sign_token   = cst::Token::from_lexical(equals_sign),
        };
    };

    auto extract_implementation(Parse_context& context) -> cst::Definition::Variant
    {
        Lexical_token const* const impl_keyword = context.pointer - 1;

        auto template_parameters = parse_template_parameters(context);
        auto type                = extract_type(context);
        auto definitions         = extract_braced_definition_sequence(context);

        return cst::definition::Implementation {
            .template_parameters = std::move(template_parameters),
            .definitions         = std::move(definitions),
            .self_type           = type,
            .impl_keyword_token  = cst::Token::from_lexical(impl_keyword),
        };
    };

    auto extract_instantiation(Parse_context& context) -> cst::Definition::Variant
    {
        Lexical_token const* const inst_keyword        = context.pointer - 1;
        auto                       template_parameters = parse_template_parameters(context);

        if (auto typeclass = parse_class_reference(context)) {
            Lexical_token const* const for_keyword = context.extract_required(Token_type::for_);

            auto type        = extract_type(context);
            auto definitions = extract_braced_definition_sequence(context);

            return cst::definition::Instantiation {
                .template_parameters = std::move(template_parameters),
                .typeclass           = std::move(*typeclass),
                .definitions         = std::move(definitions),
                .self_type           = type,
                .inst_keyword_token  = cst::Token::from_lexical(inst_keyword),
                .for_keyword_token   = cst::Token::from_lexical(for_keyword),
            };
        }
        context.error_expected("a class name");
    };

    auto extract_type_signature(Parse_context& context) -> cst::Type_signature
    {
        Lexical_token const* const alias_keyword = context.pointer - 1;
        auto                       name          = extract_upper_name(context, "an alias name");
        auto                       template_parameters = parse_template_parameters(context);
        cst::Type_signature        signature {
                   .template_parameters = std::move(template_parameters),
                   .name                = std::move(name),
                   .alias_keyword_token = cst::Token::from_lexical(alias_keyword),
        };
        if (Lexical_token const* const colon = context.try_extract(Token_type::colon)) {
            signature.classes_colon_token = cst::Token::from_lexical(colon);
            signature.classes             = extract_class_references(context);
        }
        return signature;
    }

    auto extract_class(Parse_context& context) -> cst::Definition::Variant
    {
        Lexical_token const* const class_keyword = context.pointer - 1;

        auto name                = extract_upper_name(context, "a class name");
        auto template_parameters = parse_template_parameters(context);

        std::vector<cst::Type_signature>     type_signatures;
        std::vector<cst::Function_signature> function_signatures;

        Lexical_token const* const open_brace = context.extract_required(Token_type::brace_open);

        for (;;) {
            switch (context.extract().type) {
            case Lexical_token::Type::fn:
                function_signatures.push_back(extract_function_signature(context));
                continue;
            case Lexical_token::Type::alias:
                type_signatures.push_back(extract_type_signature(context));
                continue;
            default:
                context.retreat();
                Lexical_token const* const close_brace
                    = context.extract_required(Token_type::brace_close);
                return cst::definition::Typeclass {
                    .template_parameters = std::move(template_parameters),
                    .function_signatures = std::move(function_signatures),
                    .type_signatures     = std::move(type_signatures),
                    .name                = std::move(name),
                    .class_keyword_token = cst::Token::from_lexical(class_keyword),
                    .open_brace_token    = cst::Token::from_lexical(open_brace),
                    .close_brace_token   = cst::Token::from_lexical(close_brace),
                };
            }
        }
    };

    auto extract_namespace(Parse_context& context) -> cst::Definition::Variant
    {
        Lexical_token const* const namespace_keyword = context.pointer - 1;
        auto                       name = extract_lower_name(context, "a namespace name");
        auto                       template_parameters = parse_template_parameters(context);
        utl::always_assert(!template_parameters.has_value());
        return cst::definition::Namespace {
            .template_parameters     = std::move(template_parameters),
            .definitions             = extract_braced_definition_sequence(context),
            .name                    = std::move(name),
            .namespace_keyword_token = cst::Token::from_lexical(namespace_keyword),
        };
    };

    auto parse_definition(Parse_context& context) -> std::optional<cst::Definition>
    {
        auto const definition = [&, anchor = context.pointer](cst::Definition::Variant&& value) {
            return cst::Definition {
                .value       = std::move(value),
                .source_view = context.make_source_view(anchor, context.pointer - 1),
            };
        };
        switch (context.extract().type) {
        case Lexical_token::Type::fn:
            return definition(extract_function(context));
        case Lexical_token::Type::struct_:
            return definition(extract_struct(context));
        case Lexical_token::Type::enum_:
            return definition(extract_enum(context));
        case Lexical_token::Type::alias:
            return definition(extract_alias(context));
        case Lexical_token::Type::class_:
            return definition(extract_class(context));
        case Lexical_token::Type::impl:
            return definition(extract_implementation(context));
        case Lexical_token::Type::inst:
            return definition(extract_instantiation(context));
        case Lexical_token::Type::namespace_:
            return definition(extract_namespace(context));
        default:
            context.retreat();
            return std::nullopt;
        }
    }

    auto validate_module_name_or_path(Parse_context& context, Lexical_token const& token)
        -> utl::Pooled_string
    {
        auto const module_string = token.as_string();
        if (module_string.view().contains(".."sv)) {
            context.diagnostics().error(
                token.source_view, "A module name or path can not contain '..'");
        }
        return module_string;
    }

} // namespace

auto kieli::parse(Lex_result&& lex_result) -> Parse_result
{
    Parse_context context { std::move(lex_result), cst::Node_arena::with_default_page_size() };

    std::vector<utl::Pooled_string>   module_imports;
    std::optional<utl::Pooled_string> module_name;

    if (context.try_consume(Token_type::module_)) {
        if (Lexical_token const* const name = context.try_extract(Token_type::string_literal)) {
            module_name = validate_module_name_or_path(context, *name);
        }
        else {
            context.error_expected("a module name");
        }
    }

    while (context.try_consume(Token_type::import_)) {
        if (Lexical_token const* const name = context.try_extract(Token_type::string_literal)) {
            module_imports.push_back(validate_module_name_or_path(context, *name));
        }
        else {
            context.error_expected("a module path");
        }
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
        },
    };
}
