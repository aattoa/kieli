#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>


auto libdesugar::Desugar_context::desugar(ast::Function_argument const& argument) -> hir::Function_argument {
    return { .expression = desugar(argument.expression), .name = argument.name };
}

auto libdesugar::Desugar_context::desugar(ast::Function_parameter const& parameter) -> hir::Function_parameter {
    return hir::Function_parameter {
        .pattern          = desugar(parameter.pattern),
        .type             = parameter.type.transform(desugar()),
        .default_argument = parameter.default_argument.transform(desugar()),
    };
}

auto libdesugar::Desugar_context::desugar(const ast::Self_parameter& self_parameter) -> hir::Function_parameter {
    hir::Type self_type {
        .value       = hir::type::Self {},
        .source_view = self_parameter.source_view,
    };
    if (self_parameter.is_reference.get()) {
        self_type = hir::Type {
            .value = hir::type::Reference {
                .referenced_type = wrap(std::move(self_type)),
                .mutability      = self_parameter.mutability,
            },
            .source_view = self_parameter.source_view,
        };
    }
    hir::Pattern self_pattern {
        .value = hir::pattern::Name {
            .identifier = self_variable_identifier,
            .mutability = self_parameter.is_reference.get()
                ? ast::Mutability { ast::Mutability::Concrete { .is_mutable = false }, self_parameter.source_view }
                : self_parameter.mutability,
        },
        .source_view = self_parameter.source_view,
    };
    return hir::Function_parameter {
        .pattern = std::move(self_pattern),
        .type    = std::move(self_type),
    };
}

auto libdesugar::Desugar_context::desugar(ast::Template_argument const& argument) -> hir::Template_argument {
    return {
        .value = utl::match(argument.value,
            [](ast::Mutability const& mutability) -> hir::Template_argument::Variant {
                return mutability;
            },
            [](ast::Template_argument::Wildcard const wildcard) -> hir::Template_argument::Variant {
                return hir::Template_argument::Wildcard { .source_view = wildcard.source_view };
            },
            [this](utl::Wrapper<ast::Type> const type) -> hir::Template_argument::Variant {
                return desugar(type);
            },
            [this](utl::Wrapper<ast::Expression> const expression) -> hir::Template_argument::Variant {
                error(expression->source_view, { "Constant evaluation is not supported yet" });
            }),
        .name = argument.name,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Template_parameter const& parameter) -> hir::Template_parameter {
    return {
        .value = std::visit<hir::Template_parameter::Variant>(utl::Overload {
            [this](ast::Template_parameter::Type_parameter const& type_parameter) {
                return hir::Template_parameter::Type_parameter {
                    .classes = utl::map(desugar(), type_parameter.classes)
                };
            },
            [this](ast::Template_parameter::Value_parameter const& value_parameter) {
                return hir::Template_parameter::Value_parameter {
                    .type = value_parameter.type.transform(desugar())
                };
            },
            [](ast::Template_parameter::Mutability_parameter const&) {
                return hir::Template_parameter::Mutability_parameter {};
            }
        }, parameter.value),
        .name             = parameter.name,
        .default_argument = parameter.default_argument.transform(desugar()),
        .source_view      = parameter.source_view,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Qualifier const& qualifier) -> hir::Qualifier {
    return {
        .template_arguments = qualifier.template_arguments.transform(utl::map(desugar())),
        .name               = qualifier.name,
        .source_view        = qualifier.source_view,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Qualified_name const& name) -> hir::Qualified_name {
    return {
        .middle_qualifiers = utl::map(desugar(), name.middle_qualifiers),
        .root_qualifier = std::visit(utl::Overload {
            [](std::monostate)                         -> hir::Root_qualifier { return {}; },
            [](ast::Global_root_qualifier)             -> hir::Root_qualifier { return { .value = ast::Global_root_qualifier {} }; },
            [this](utl::Wrapper<ast::Type> const type) -> hir::Root_qualifier { return { .value = desugar(type) }; }
        }, name.root_qualifier.value),
        .primary_name = name.primary_name,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Class_reference const& reference) -> hir::Class_reference {
    return {
        .template_arguments = reference.template_arguments.transform(utl::map(desugar())),
        .name = desugar(reference.name),
        .source_view = reference.source_view,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Function_signature const& signature) -> hir::Function_signature {
    return {
        .parameters     = utl::map(desugar(), signature.parameters),
        .self_parameter = signature.self_parameter,
        .return_type    = signature.return_type.transform(desugar()),
        .name           = signature.name,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Function_template_signature const& signature) -> hir::Function_template_signature {
    return {
        .function_signature  = desugar(signature.function_signature),
        .template_parameters = utl::map(desugar(), signature.template_parameters)
    };
}

auto libdesugar::Desugar_context::desugar(ast::Type_signature const& signature) -> hir::Type_signature {
    return {
        .classes = utl::map(desugar(), signature.classes),
        .name    = signature.name,
    };
}

auto libdesugar::Desugar_context::desugar(ast::Type_template_signature const& signature) -> hir::Type_template_signature {
    return {
        .type_signature      = desugar(signature.type_signature),
        .template_parameters = utl::map(desugar(), signature.template_parameters),
    };
}


auto libdesugar::Desugar_context::unit_value(utl::Source_view const view) -> utl::Wrapper<hir::Expression> {
    return wrap(hir::Expression { .value = hir::expression::Tuple {}, .source_view = view });
}
auto libdesugar::Desugar_context::wildcard_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return wrap(hir::Pattern { .value = hir::pattern::Wildcard {}, .source_view = view});
}
auto libdesugar::Desugar_context::true_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return wrap(hir::Pattern { .value = hir::pattern::Literal<kieli::Boolean> { true }, .source_view = view });
}
auto libdesugar::Desugar_context::false_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return wrap(hir::Pattern { .value = hir::pattern::Literal<kieli::Boolean> { false }, .source_view = view });
}


auto libdesugar::Desugar_context::error(utl::Source_view const erroneous_view, utl::diagnostics::Message_arguments const arguments) -> void {
    compilation_info.get()->diagnostics.emit_error(arguments.add_source_view(erroneous_view));
}


auto kieli::desugar(kieli::Parse_result&& parse_result) -> Desugar_result {
    (void)parse_result;
    utl::todo();
#if 0
    libdesugar::Desugar_context context {
        std::move(parse_result.compilation_info),
        hir::Node_arena::with_default_page_size(),
    };
    auto desugared_definitions = utl::map(context.desugar(), parse_result.module.definitions);
    return Desugar_result {
        .compilation_info = std::move(context.compilation_info),
        .node_arena       = std::move(context.node_arena),
        .module           = { .definitions = std::move(desugared_definitions) },
    };
#endif
}
