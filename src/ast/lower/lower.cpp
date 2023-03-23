#include "utl/utilities.hpp"
#include "lower.hpp"
#include "lowering_internals.hpp"
#include "mir/mir.hpp"


Lowering_context::Lowering_context(
    hir::Node_context            & node_context,
    utl::diagnostics::Builder     & diagnostics,
    utl::Source              const& source,
    compiler::Program_string_pool& string_pool
) noexcept
    : node_context { node_context }
    , diagnostics  { diagnostics }
    , source       { source }
    , string_pool  { string_pool } {}


auto Lowering_context::is_within_function() const noexcept -> bool {
    return current_definition_kind
        == utl::alternative_index<ast::Definition::Variant, ast::definition::Function>;
}

auto Lowering_context::fresh_name_tag() -> utl::Usize {
    return current_name_tag++.get();
}


auto Lowering_context::lower(ast::Function_argument const& argument) -> hir::Function_argument {
    return { .expression = lower(argument.expression), .name = argument.name };
}

auto Lowering_context::lower(ast::Function_parameter const& parameter) -> hir::Function_parameter {
    return hir::Function_parameter {
        .pattern = lower(parameter.pattern),
        .type    = std::invoke([this, &parameter]() -> hir::Type {
            if (parameter.type) {
                return lower(*parameter.type);
            }
            else {
                utl::always_assert(current_function_implicit_template_parameters != nullptr);
                auto const tag = fresh_name_tag();
                current_function_implicit_template_parameters->push_back({
                    .tag = hir::Implicit_template_parameter::Tag { tag } });
                return {
                    .value = hir::type::Implicit_parameter_reference {
                        .tag = hir::Implicit_template_parameter::Tag { tag } },
                    .source_view = parameter.pattern.source_view
                };
            }
        }),
        .default_value = parameter.default_value.transform(lower())
    };
}

auto Lowering_context::lower(ast::Template_argument const& argument) -> hir::Template_argument {
    return {
        .value = utl::match(
            argument.value,

            [](ast::Mutability const& mutability) -> hir::Template_argument::Variant {
                return mutability;
            },
            [](ast::Template_argument::Wildcard const wildcard) -> hir::Template_argument::Variant {
                return hir::Template_argument::Wildcard { .source_view = wildcard.source_view };
            },
            [this](utl::Wrapper<ast::Type> const type) -> hir::Template_argument::Variant {
                return lower(type);
            },
            [this](utl::Wrapper<ast::Expression> const expression) -> hir::Template_argument::Variant {
                error(expression->source_view, { "Constant evaluation is not supported yet" });
            }
        ),
        .name = argument.name,
    };
}

auto Lowering_context::lower(ast::Template_parameter const& parameter) -> hir::Template_parameter {
    return {
        .value = std::visit<hir::Template_parameter::Variant>(utl::Overload {
            [this](ast::Template_parameter::Type_parameter const& type_parameter) {
                return hir::Template_parameter::Type_parameter {
                    .classes = utl::map(lower())(type_parameter.classes)
                };
            },
            [this](ast::Template_parameter::Value_parameter const& value_parameter) {
                return hir::Template_parameter::Value_parameter {
                    .type = value_parameter.type.transform(lower())
                };
            },
            [](ast::Template_parameter::Mutability_parameter const&) {
                return hir::Template_parameter::Mutability_parameter {};
            }
        }, parameter.value),
        .name             = parameter.name,
        .default_argument = parameter.default_argument.transform(lower()),
        .source_view      = parameter.source_view
    };
}

auto Lowering_context::lower(ast::Qualifier const& qualifier) -> hir::Qualifier {
    return {
        .template_arguments = qualifier.template_arguments.transform(utl::map(lower())),
        .name               = qualifier.name,
        .source_view        = qualifier.source_view
    };
}

auto Lowering_context::lower(ast::Qualified_name const& name) -> hir::Qualified_name {
    return {
        .middle_qualifiers = utl::map(lower())(name.middle_qualifiers),
        .root_qualifier = std::visit(utl::Overload {
            [](std::monostate)                        -> hir::Root_qualifier { return {}; },
            [](ast::Root_qualifier::Global)           -> hir::Root_qualifier { return { .value = hir::Root_qualifier::Global {} }; },
            [this](utl::Wrapper<ast::Type> const type) -> hir::Root_qualifier { return { .value = lower(type) }; }
        }, name.root_qualifier.value),
        .primary_name = name.primary_name,
    };
}

auto Lowering_context::lower(ast::Class_reference const& reference) -> hir::Class_reference {
    return {
        .template_arguments = reference.template_arguments.transform(utl::map(lower())),
        .name = lower(reference.name),
        .source_view = reference.source_view,
    };
}

auto Lowering_context::lower(ast::Function_signature const& signature) -> hir::Function_signature {
    return {
        .parameter_types = utl::map(lower())(signature.parameter_types),
        .return_type     = lower(signature.return_type),
        .name            = signature.name,
    };
}

auto Lowering_context::lower(ast::Function_template_signature const& signature) -> hir::Function_template_signature {
    return {
        .function_signature  = lower(signature.function_signature),
        .template_parameters = utl::map(lower())(signature.template_parameters)
    };
}

auto Lowering_context::lower(ast::Type_signature const& signature) -> hir::Type_signature {
    return {
        .classes = utl::map(lower())(signature.classes),
        .name    = signature.name,
    };
}

auto Lowering_context::lower(ast::Type_template_signature const& signature) -> hir::Type_template_signature {
    return {
        .type_signature      = lower(signature.type_signature),
        .template_parameters = utl::map(lower())(signature.template_parameters)
    };
}


auto Lowering_context::unit_value(utl::Source_view const view) -> utl::Wrapper<hir::Expression> {
    return utl::wrap(hir::Expression { .value = hir::expression::Tuple {}, .source_view = view });
}
auto Lowering_context::wildcard_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return utl::wrap(hir::Pattern { .value = hir::pattern::Wildcard {}, .source_view = view});
}
auto Lowering_context::true_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return utl::wrap(hir::Pattern { .value = hir::pattern::Literal<bool> { true }, .source_view = view });
}
auto Lowering_context::false_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return utl::wrap(hir::Pattern { .value = hir::pattern::Literal<bool> { false }, .source_view = view });
}


auto Lowering_context::error(
    utl::Source_view                    const erroneous_view,
    utl::diagnostics::Message_arguments const arguments) const -> void
{
    diagnostics.emit_simple_error(arguments.add_source_info(source, erroneous_view));
}


auto compiler::lower(Parse_result&& parse_result) -> Lower_result {
    Lower_result lower_result {
        .node_context = hir::Node_context {
            utl::Wrapper_context<hir::Expression> { parse_result.node_context.arena_size<ast::Expression>() },
            utl::Wrapper_context<hir::Type>       { parse_result.node_context.arena_size<ast::Type>      () },
            utl::Wrapper_context<hir::Pattern>    { parse_result.node_context.arena_size<ast::Pattern>   () }
        },
        .diagnostics = std::move(parse_result.diagnostics),
        .source      = std::move(parse_result.source),
        .string_pool = parse_result.string_pool
    };

    Lowering_context context {
        lower_result.node_context,
        lower_result.diagnostics,
        lower_result.source,
        lower_result.string_pool
    };

    lower_result.module.definitions =
        utl::map(context.lower())(parse_result.module.definitions);

    return lower_result;
}