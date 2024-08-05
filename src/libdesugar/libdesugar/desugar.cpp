#include <libutl/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>

auto libdesugar::Context::deref_desugar(cst::Expression_id const id) -> ast::Expression
{
    return desugar(cst.expressions[id]);
}

auto libdesugar::Context::deref_desugar(cst::Pattern_id const id) -> ast::Pattern
{
    return desugar(cst.patterns[id]);
}

auto libdesugar::Context::deref_desugar(cst::Type_id const id) -> ast::Type
{
    return desugar(cst.types[id]);
}

auto libdesugar::Context::desugar(cst::Expression_id const id) -> utl::Wrapper<ast::Expression>
{
    return wrap(deref_desugar(id));
}

auto libdesugar::Context::desugar(cst::Pattern_id const id) -> utl::Wrapper<ast::Pattern>
{
    return wrap(deref_desugar(id));
}

auto libdesugar::Context::desugar(cst::Type_id const id) -> utl::Wrapper<ast::Type>
{
    return wrap(deref_desugar(id));
}

auto libdesugar::Context::desugar(cst::Function_argument const& argument) -> ast::Function_argument
{
    return ast::Function_argument {
        .expression = desugar(argument.expression),
        .name       = argument.name.transform([](auto const& syntax) { return syntax.name; }),
    };
}

auto libdesugar::Context::desugar(cst::Function_parameter const& parameter)
    -> ast::Function_parameter
{
    return ast::Function_parameter {
        .pattern          = desugar(parameter.pattern),
        .type             = parameter.type.transform(wrap_desugar()),
        .default_argument = parameter.default_argument.transform(
            [&](cst::Value_parameter_default_argument const& argument) {
                if (auto const* const wildcard = std::get_if<cst::Wildcard>(&argument.variant)) {
                    kieli::fatal_error(
                        db,
                        source,
                        wildcard->range,
                        "A default function argument may not be a wildcard");
                }
                return desugar(std::get<cst::Expression_id>(argument.variant));
            }),
    };
}

auto libdesugar::Context::desugar(cst::Wildcard const& wildcard) -> ast::Wildcard
{
    return ast::Wildcard { .range = wildcard.range };
}

auto libdesugar::Context::desugar(cst::Self_parameter const& self_parameter) -> ast::Self_parameter
{
    return ast::Self_parameter {
        .mutability
        = desugar_mutability(self_parameter.mutability, self_parameter.self_keyword_token.range),
        .is_reference = self_parameter.is_reference(),
        .range        = self_parameter.self_keyword_token.range,
    };
}

auto libdesugar::Context::desugar(cst::Template_argument const& argument) -> ast::Template_argument
{
    return std::visit<ast::Template_argument>(desugar(), argument);
}

auto libdesugar::Context::desugar(cst::Template_parameter const& template_parameter)
    -> ast::Template_parameter
{
    return {
        std::visit<ast::Template_parameter_variant>(
            utl::Overload {
                [&](cst::Template_type_parameter const& parameter) {
                    return ast::Template_type_parameter {
                        .name             = parameter.name,
                        .concepts         = desugar(parameter.concepts),
                        .default_argument = parameter.default_argument.transform(desugar()),
                    };
                },
                [&](cst::Template_value_parameter const& parameter) {
                    return ast::Template_value_parameter {
                        .name             = parameter.name,
                        .type             = parameter.type_annotation.transform(wrap_desugar()),
                        .default_argument = parameter.default_argument.transform(desugar()),
                    };
                },
                [&](cst::Template_mutability_parameter const& parameter) {
                    return ast::Template_mutability_parameter {
                        .name             = parameter.name,
                        .default_argument = parameter.default_argument.transform(desugar()),
                    };
                },
            },
            template_parameter.variant),
        template_parameter.range,
    };
}

auto libdesugar::Context::desugar(cst::Path_segment const& segment) -> ast::Path_segment
{
    return ast::Path_segment {
        .template_arguments = segment.template_arguments.transform(desugar()),
        .name               = segment.name,
    };
}

auto libdesugar::Context::desugar(cst::Path const& path) -> ast::Path
{
    return ast::Path {
        .segments = desugar(path.segments.elements),
        .root     = path.root.transform([&](cst::Path_root const& root) {
            return std::visit(
                utl::Overload {
                    [](cst::Path_root_global const&) -> ast::Path_root {
                        return ast::Path_root_global {};
                    },
                    [&](cst::Type_id const type) -> ast::Path_root {
                        return desugar(type); //
                    },
                },
                root.variant);
        }),
        .head = path.head,
    };
}

auto libdesugar::Context::desugar(cst::Concept_reference const& reference) -> ast::Concept_reference
{
    return ast::Concept_reference {
        .template_arguments = reference.template_arguments.transform(desugar()),
        .path               = desugar(reference.path),
        .range              = reference.range,
    };
}

auto libdesugar::Context::desugar(cst::Function_signature const& signature)
    -> ast::Function_signature
{
    std::vector<ast::Function_parameter> parameters;
    if (signature.function_parameters.value.self_parameter.has_value()) {
        parameters.push_back(
            normalize_self_parameter(signature.function_parameters.value.self_parameter.value()));
    }
    parameters.append_range(std::views::transform(
        signature.function_parameters.value.normal_parameters.elements, desugar()));

    ast::Type return_type = signature.return_type.has_value()
                              ? desugar(cst.types[signature.return_type.value().type])
                              : unit_type(signature.name.range);

    return ast::Function_signature {
        .template_parameters = signature.template_parameters.transform(desugar()),
        .function_parameters = std::move(parameters),
        .self_parameter = signature.function_parameters.value.self_parameter.transform(desugar()),
        .return_type    = std::move(return_type),
        .name           = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::Type_signature const& signature) -> ast::Type_signature
{
    return ast::Type_signature {
        .concepts = desugar(signature.concepts.elements),
        .name     = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::expression::Struct_initializer::Field const& field)
    -> ast::expression::Struct_initializer::Field
{
    return { .name = field.name, .expression = desugar(field.expression) };
}

auto libdesugar::Context::desugar(cst::Mutability const& mutability) -> ast::Mutability
{
    return ast::Mutability {
        .variant = std::visit<ast::Mutability_variant>(
            utl::Overload {
                [](cst::mutability::Concrete const concrete) -> ast::mutability::Concrete {
                    return concrete;
                },
                [](cst::mutability::Parameterized const parameterized) {
                    return ast::mutability::Parameterized { parameterized.name };
                },
            },
            mutability.variant),
        .range = mutability.range,
    };
}

auto libdesugar::Context::desugar(cst::pattern::Field const& field) -> ast::pattern::Field
{
    return ast::pattern::Field {
        .name    = field.name,
        .pattern = field.field_pattern.transform(
            [&](cst::pattern::Field::Field_pattern const& field_pattern) {
                return desugar(field_pattern.pattern);
            }),
    };
}

auto libdesugar::Context::desugar(cst::pattern::Constructor_body const& body)
    -> ast::pattern::Constructor_body
{
    return std::visit<ast::pattern::Constructor_body>(
        utl::Overload {
            [&](cst::pattern::Struct_constructor const& constructor) {
                return ast::pattern::Struct_constructor { .fields = desugar(constructor.fields) };
            },
            [&](cst::pattern::Tuple_constructor const& constructor) {
                return ast::pattern::Tuple_constructor { .pattern = desugar(constructor.pattern) };
            },
            [&](cst::pattern::Unit_constructor const&) {
                return ast::pattern::Unit_constructor {};
            },
        },
        body);
};

auto libdesugar::Context::desugar(cst::Type_annotation const& annotation) -> ast::Type
{
    return desugar(cst.types[annotation.type]);
}

auto libdesugar::Context::desugar_mutability(
    std::optional<cst::Mutability> const& mutability, kieli::Range const range) -> ast::Mutability
{
    if (mutability.has_value()) {
        return desugar(mutability.value());
    }
    return ast::Mutability {
        .variant = ast::mutability::Concrete::immut,
        .range   = range,
    };
}

auto libdesugar::Context::normalize_self_parameter(cst::Self_parameter const& self_parameter)
    -> ast::Function_parameter
{
    ast::Type self_type {
        .variant = ast::type::Self {},
        .range   = self_parameter.self_keyword_token.range,
    };
    if (self_parameter.is_reference()) {
        self_type = ast::Type {
            ast::type::Reference {
                .referenced_type = wrap(std::move(self_type)),
                .mutability      = desugar_mutability(
                    self_parameter.mutability, self_parameter.self_keyword_token.range),
            },
            self_parameter.self_keyword_token.range,
        };
    }
    return ast::Function_parameter {
        .pattern = wrap(ast::Pattern {
            ast::pattern::Name {
                .name { kieli::Name {
                    .identifier = self_variable_identifier,
                    .range      = self_parameter.self_keyword_token.range,
                } },
                .mutability = desugar_mutability(
                    self_parameter.is_reference() ? std::nullopt : self_parameter.mutability,
                    self_parameter.self_keyword_token.range),
            },
            self_parameter.self_keyword_token.range,
        }),
        .type    = wrap(std::move(self_type)),
    };
}

auto libdesugar::unit_type(kieli::Range const range) -> ast::Type
{
    return ast::Type { ast::type::Tuple {}, range };
}

auto libdesugar::unit_value(kieli::Range const range) -> ast::Expression
{
    return ast::Expression { ast::expression::Tuple {}, range };
}

auto libdesugar::wildcard_pattern(kieli::Range const range) -> ast::Pattern
{
    return ast::Pattern { ast::Wildcard { range }, range };
}

auto kieli::desugar(CST const& cst, Database& db) -> AST
{
    auto node_arena = ast::Node_arena::with_default_page_size();
    auto context    = libdesugar::Context { db, cst.module->arena, node_arena, cst.module->source };
    auto definitions = context.desugar(cst.module->definitions);
    return AST { AST::Module { std::move(definitions), std::move(node_arena) } };
}
