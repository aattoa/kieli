#include <libutl/common/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/desugaring_internals.hpp>


auto libdesugar::Desugar_context::desugar(cst::Function_argument const& argument) -> hir::Function_argument {
    return {
        .expression    = desugar(argument.expression),
        .argument_name = argument.argument_name.transform(&cst::Name_lower_equals::name),
    };
}

auto libdesugar::Desugar_context::desugar(cst::Function_parameter const& parameter) -> hir::Function_parameter {
    return hir::Function_parameter {
        .pattern          = desugar(parameter.pattern),
        .type             = parameter.type.transform(utl::compose(wrap(), desugar())),
        .default_argument = parameter.default_argument.transform(desugar()),
    };
}

auto libdesugar::Desugar_context::desugar(const cst::Self_parameter& self_parameter) -> hir::Self_parameter {
    return hir::Self_parameter {
        .mutability   = desugar_mutability(self_parameter.mutability, self_parameter.self_keyword_token.source_view),
        .is_reference = self_parameter.is_reference(),
        .source_view  = self_parameter.source_view,
    };
}

auto libdesugar::Desugar_context::desugar(cst::Template_argument const& argument) -> hir::Template_argument {
    return {
        .value = utl::match(argument.value,
            [this](cst::Mutability const& mutability) -> hir::Template_argument::Variant {
                return desugar(mutability);
            },
            [](cst::Template_argument::Wildcard const wildcard) -> hir::Template_argument::Variant {
                return hir::Template_argument::Wildcard { .source_view = wildcard.source_view };
            },
            [this](utl::Wrapper<cst::Type> const type) -> hir::Template_argument::Variant {
                return desugar(type);
            },
            [this](utl::Wrapper<cst::Expression> const expression) -> hir::Template_argument::Variant {
                error(expression->source_view, { "Constant evaluation is not supported yet" });
            }),
    };
}

auto libdesugar::Desugar_context::desugar(cst::Template_parameter const& parameter) -> hir::Template_parameter {
    return {
        .value = utl::match(parameter.value,
            [this](cst::Template_parameter::Type_parameter const& type_parameter) -> hir::Template_parameter::Variant {
                return hir::Template_parameter::Type_parameter {
                    .classes = utl::map(desugar(), type_parameter.classes.elements),
                    .name    = type_parameter.name,
                };
            },
            [this](cst::Template_parameter::Value_parameter const& value_parameter) -> hir::Template_parameter::Variant {
                return hir::Template_parameter::Value_parameter {
                    .type = value_parameter.type.transform(desugar()),
                    .name = value_parameter.name,
                };
            },
            [](cst::Template_parameter::Mutability_parameter const& mutability_parameter) -> hir::Template_parameter::Variant {
                return hir::Template_parameter::Mutability_parameter {
                    .name = mutability_parameter.name,
                };
            }),
        .default_argument = parameter.default_argument.transform(desugar()),
        .source_view      = parameter.source_view,
    };
}

auto libdesugar::Desugar_context::desugar(cst::Qualifier const&) -> hir::Qualifier {
    utl::todo();
}

auto libdesugar::Desugar_context::desugar(cst::Qualified_name const& name) -> hir::Qualified_name {
    return {
        .middle_qualifiers = utl::map(desugar(), name.middle_qualifiers.elements),
        .root_qualifier = name.root_qualifier.transform([&](cst::Root_qualifier const& root) {
            return utl::match(root.value,
                [](std::monostate)                         -> hir::Root_qualifier { return {}; },
                [](cst::Root_qualifier::Global)            -> hir::Root_qualifier { return { .value = hir::Root_qualifier::Global {} }; },
                [this](utl::Wrapper<cst::Type> const type) -> hir::Root_qualifier { return { .value = desugar(type) }; });
        }),
        .primary_name = name.primary_name,
    };
}

auto libdesugar::Desugar_context::desugar(cst::Class_reference const&) -> hir::Class_reference {
    utl::todo();
}

auto libdesugar::Desugar_context::desugar(cst::Function_signature const& signature) -> hir::Function_signature {
    return {
        .function_parameters = utl::map(desugar(), signature.function_parameters.normal_parameters.elements),
        .self_parameter      = signature.function_parameters.self_parameter.transform(desugar()),
        .return_type         = signature.return_type.transform(desugar()),
        .name                = signature.name,
    };
}

auto libdesugar::Desugar_context::desugar(cst::Type_signature const& signature) -> hir::Type_signature {
    return {
        .classes = utl::map(desugar(), signature.classes.elements),
        .name    = signature.name,
    };
}

auto libdesugar::Desugar_context::desugar(cst::Mutability const& mutability) -> hir::Mutability {
    return hir::Mutability {
        .value = utl::match(mutability.value,
            [](cst::Mutability::Concrete const concrete) -> hir::Mutability::Variant {
                return hir::Mutability::Concrete { .is_mutable = concrete.is_mutable };
            },
            [](cst::Mutability::Parameterized const parameterized) -> hir::Mutability::Variant {
                return hir::Mutability::Parameterized { .name = parameterized.name };
            }),
        .is_explicit = true,
        .source_view = mutability.source_view,
    };
}

auto libdesugar::Desugar_context::desugar(cst::Type_annotation const& annotation) -> hir::Type {
    return desugar(*annotation.type);
}
auto libdesugar::Desugar_context::desugar(cst::Function_parameter::Default_argument const& default_argument) -> utl::Wrapper<hir::Expression> {
    return desugar(default_argument.expression);
}
auto libdesugar::Desugar_context::desugar(cst::Template_parameter::Default_argument const& default_argument) -> hir::Template_argument {
    return desugar(default_argument.argument);
}

auto libdesugar::Desugar_context::desugar_mutability(
    tl::optional<cst::Mutability> const& mutability,
    utl::Source_view const               source_view) -> hir::Mutability
{
    if (mutability.has_value()) {
        return desugar(*mutability);
    }
    return hir::Mutability {
        .value       = hir::Mutability::Concrete { .is_mutable = false },
        .is_explicit = false,
        .source_view = source_view,
    };
}

auto libdesugar::Desugar_context::normalize_self_parameter(cst::Self_parameter const& self_parameter)
    -> hir::Function_parameter
{
    hir::Type self_type {
        .value       = hir::type::Self {},
        .source_view = self_parameter.source_view,
    };
    if (self_parameter.is_reference()) {
        self_type = hir::Type {
            .value = hir::type::Reference {
                .referenced_type = wrap(std::move(self_type)),
                .mutability      = desugar_mutability(self_parameter.mutability, self_parameter.self_keyword_token.source_view),
            },
            .source_view = self_parameter.source_view,
        };
    }
    hir::Pattern self_pattern {
        .value = hir::pattern::Name {
            .name {
                .identifier  = self_variable_identifier,
                .source_view = self_parameter.source_view,
            },
            .mutability = desugar_mutability(
                self_parameter.is_reference()
                    ? tl::nullopt
                    : self_parameter.mutability,
                self_parameter.source_view),
        },
        .source_view = self_parameter.source_view,
    };
    return hir::Function_parameter {
        .pattern = wrap(std::move(self_pattern)),
        .type    = wrap(std::move(self_type)),
    };
}

auto libdesugar::Desugar_context::unit_value(utl::Source_view const view) -> utl::Wrapper<hir::Expression> {
    return wrap(hir::Expression { .value = hir::expression::Tuple {}, .source_view = view });
}
auto libdesugar::Desugar_context::wildcard_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return wrap(hir::Pattern { .value = hir::pattern::Wildcard {}, .source_view = view});
}
auto libdesugar::Desugar_context::true_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return wrap(hir::Pattern { .value = hir::pattern::Literal<compiler::Boolean> { true }, .source_view = view });
}
auto libdesugar::Desugar_context::false_pattern(utl::Source_view const view) -> utl::Wrapper<hir::Pattern> {
    return wrap(hir::Pattern { .value = hir::pattern::Literal<compiler::Boolean> { false }, .source_view = view });
}


auto libdesugar::Desugar_context::error(utl::Source_view const erroneous_view, utl::diagnostics::Message_arguments const arguments) -> void {
    compilation_info.get()->diagnostics.emit_error(arguments.add_source_view(erroneous_view));
}


auto kieli::desugar(kieli::Parse_result&& parse_result) -> Desugar_result {
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
}
