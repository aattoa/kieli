#include <libutl/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>

auto libdesugar::Context::wrap_desugar(cst::Expression const& expression) -> ast::Expression_id
{
    return ast.expressions.push(desugar(expression));
}

auto libdesugar::Context::wrap_desugar(cst::Pattern const& pattern) -> ast::Pattern_id
{
    return ast.patterns.push(desugar(pattern));
}

auto libdesugar::Context::wrap_desugar(cst::Type const& type) -> ast::Type_id
{
    return ast.types.push(desugar(type));
}

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

auto libdesugar::Context::desugar(cst::Expression_id const id) -> ast::Expression_id
{
    return ast.expressions.push(deref_desugar(id));
}

auto libdesugar::Context::desugar(cst::Pattern_id const id) -> ast::Pattern_id
{
    return ast.patterns.push(deref_desugar(id));
}

auto libdesugar::Context::desugar(cst::Type_id const id) -> ast::Type_id
{
    return ast.types.push(deref_desugar(id));
}

auto libdesugar::Context::desugar(cst::Wildcard const& wildcard) -> ast::Wildcard
{
    return ast::Wildcard { .range = cst.tokens[wildcard.underscore_token].range };
}

auto libdesugar::Context::desugar(cst::Template_argument const& argument) -> ast::Template_argument
{
    return std::visit<ast::Template_argument>(desugar(), argument);
}

auto libdesugar::Context::desugar(cst::Template_parameter const& template_parameter)
    -> ast::Template_parameter
{
    auto const visitor = utl::Overload {
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
                .type             = desugar(parameter.type_annotation),
                .default_argument = parameter.default_argument.transform(desugar()),
            };
        },
        [&](cst::Template_mutability_parameter const& parameter) {
            return ast::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(desugar()),
            };
        },
    };
    return {
        std::visit<ast::Template_parameter_variant>(visitor, template_parameter.variant),
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
    auto const root_visitor = utl::Overload {
        [](std::monostate) { return std::monostate {}; },
        [](cst::Path_root_global const&) { return ast::Path_root_global {}; },
        [&](cst::Type_id const type) { return desugar(type); },
    };
    return ast::Path {
        .root     = std::visit<ast::Path_root>(root_visitor, path.root),
        .segments = desugar(path.segments),
    };
}

auto libdesugar::Context::desugar(cst::Function_parameters const& cst_parameters)
    -> std::vector<ast::Function_parameter>
{
    std::vector<ast::Function_parameter> ast_parameters;
    ast_parameters.reserve(cst_parameters.value.elements.size());

    auto const desugar_argument = [&](cst::Value_parameter_default_argument const& argument) {
        if (auto const* const wildcard = std::get_if<cst::Wildcard>(&argument.variant)) {
            auto const range = cst.tokens[wildcard->underscore_token].range;
            kieli::add_error(db, document_id, range, "A default argument may not be a wildcard");
            return ast.expressions.push(ast::Error {}, range);
        }
        return desugar(std::get<cst::Expression_id>(argument.variant));
    };

    auto const desugar_type = [&](cst::Function_parameter const& parameter) {
        if (parameter.type.has_value()) {
            return desugar(parameter.type.value());
        }
        if (!ast_parameters.empty()) {
            return ast_parameters.front().type;
        }
        auto const range = cst.patterns[parameter.pattern].range;
        kieli::add_error(
            db, document_id, range, "The type of the final parameter must not be omitted");
        return ast.types.push(ast::Error {}, range);
    };

    for (auto const& parameter : std::views::reverse(cst_parameters.value.elements)) {
        ast_parameters.emplace(
            ast_parameters.begin(),
            desugar(parameter.pattern),
            desugar_type(parameter),
            parameter.default_argument.transform(desugar_argument));
    }

    return ast_parameters;
}

auto libdesugar::Context::desugar(cst::Function_signature const& signature)
    -> ast::Function_signature
{
    // If there is no explicit return type, insert the unit type.
    ast::Type return_type = signature.return_type.has_value()
                              ? desugar(cst.types[signature.return_type.value().type])
                              : unit_type(signature.name.range);

    return ast::Function_signature {
        .template_parameters = signature.template_parameters.transform(desugar()),
        .function_parameters = desugar(signature.function_parameters),
        .return_type         = std::move(return_type),
        .name                = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::Type_signature const& signature) -> ast::Type_signature
{
    return ast::Type_signature {
        .concepts = desugar(signature.concepts.elements),
        .name     = signature.name,
    };
}

auto libdesugar::Context::desugar(cst::Struct_field_initializer const& field)
    -> ast::Struct_field_initializer
{
    if (field.equals.has_value()) {
        return { field.name, desugar(field.equals.value().expression) };
    }
    ast::Path path { .segments = utl::to_vector({ ast::Path_segment { .name = field.name } }) };
    return { field.name, ast.expressions.push(std::move(path), field.name.range) };
}

auto libdesugar::Context::desugar(cst::Mutability const& mutability) -> ast::Mutability
{
    auto const visitor = utl::Overload {
        [](kieli::Mutability const concrete) { return concrete; },
        [](cst::Parameterized_mutability const& parameterized) {
            return ast::Parameterized_mutability { parameterized.name };
        },
    };
    return ast::Mutability {
        .variant = std::visit<ast::Mutability_variant>(visitor, mutability.variant),
        .range   = mutability.range,
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
    auto const visitor = utl::Overload {
        [&](cst::pattern::Struct_constructor const& constructor) {
            return ast::pattern::Struct_constructor { .fields = desugar(constructor.fields) };
        },
        [&](cst::pattern::Tuple_constructor const& constructor) {
            return ast::pattern::Tuple_constructor { .pattern = desugar(constructor.pattern) };
        },
        [&](cst::pattern::Unit_constructor const&) { return ast::pattern::Unit_constructor {}; },
    };
    return std::visit<ast::pattern::Constructor_body>(visitor, body);
};

auto libdesugar::Context::desugar(cst::Type_annotation const& annotation) -> ast::Type_id
{
    return desugar(annotation.type);
}

auto libdesugar::Context::desugar_mutability(
    std::optional<cst::Mutability> const& mutability, kieli::Range const range) -> ast::Mutability
{
    if (mutability.has_value()) {
        return desugar(mutability.value());
    }
    return ast::Mutability { kieli::Mutability::immut, range };
}

auto libdesugar::unit_type(kieli::Range const range) -> ast::Type
{
    return ast::Type { ast::type::Tuple {}, range };
}

auto libdesugar::wildcard_type(kieli::Range const range) -> ast::Type
{
    return ast::Type { ast::Wildcard { range }, range };
}

auto libdesugar::unit_value(kieli::Range const range) -> ast::Expression
{
    return ast::Expression { ast::expression::Tuple {}, range };
}

auto libdesugar::wildcard_pattern(kieli::Range const range) -> ast::Pattern
{
    return ast::Pattern { ast::Wildcard { range }, range };
}
