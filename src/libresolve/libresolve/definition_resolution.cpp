#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>


namespace {

    using namespace resolution;


    enum class Allow_generalization { yes, no };


    auto resolve_function_parameters(
        Context                                & context,
        Allow_generalization               const allow_generalization,
        std::span<hir::Function_parameter> const hir_parameters,
        std::vector<mir::Template_parameter>   & template_parameters,
        Scope                                    signature_scope,
        Namespace                              & home_namespace) -> utl::Pair<Scope, std::vector<mir::Function_parameter>>
    {
        std::vector<mir::Function_parameter> mir_parameters;
        mir_parameters.reserve(hir_parameters.size());

        for (hir::Function_parameter& parameter : hir_parameters) {
            if (!parameter.type.has_value())
                context.error(parameter.pattern.source_view, { "Implicit parameter types are not supported yet" });
            if (parameter.default_argument.has_value())
                context.error(parameter.default_argument->source_view, { "Default arguments are not supported yet" });

            mir::Type const parameter_type =
                context.resolve_type(utl::get(parameter.type), signature_scope, home_namespace);
            mir::Pattern parameter_pattern =
                context.resolve_pattern(parameter.pattern, signature_scope, home_namespace);

            if (!parameter_pattern.is_exhaustive_by_itself)
                context.error(parameter_pattern.source_view, { "Inexhaustive function parameter pattern" });

            context.solve(constraint::Type_equality {
                .constrainer_type = parameter_type,
                .constrained_type = parameter_pattern.type,
                .constrainer_note = constraint::Explanation {
                    parameter_type.source_view(),
                    "This parameter declared to be of type {0}"
                },
                .constrained_note {
                    parameter_pattern.source_view,
                    "But its pattern is of type {1}"
                }
            });

            if (allow_generalization == Allow_generalization::yes)
                context.generalize_to(parameter_type, template_parameters);

            mir_parameters.push_back(mir::Function_parameter {
                .pattern = std::move(parameter_pattern),
                .type    = parameter_type,
            });
        }

        return { std::move(signature_scope), std::move(mir_parameters) };
    }


    auto resolve_self_parameter(Context& context, Scope& scope, tl::optional<ast::Self_parameter> const& self)
        -> tl::optional<mir::Self_parameter>
    {
        return self.transform([&](ast::Self_parameter const& self) {
            return mir::Self_parameter {
                .mutability   = context.resolve_mutability(self.mutability, scope),
                .is_reference = self.is_reference,
                .source_view  = self.source_view,
            };
        });
    }


    auto make_function_signature(
        Context&                               context,
        ast::Name                        const function_name,
        mir::Type                        const return_type,
        tl::optional<mir::Self_parameter>   && self_parameter,
        std::vector<mir::Function_parameter>&& function_parameters,
        std::vector<mir::Template_parameter>&& template_parameters) -> mir::Function::Signature
    {
        mir::Type const function_type {
            context.wrap_type(mir::type::Function {
                .parameter_types = utl::map(&mir::Function_parameter::type, function_parameters),
                .return_type     = return_type,
            }),
            function_name.source_view,
        };
        return mir::Function::Signature {
            .template_parameters = std::move(template_parameters),
            .parameters          = std::move(function_parameters),
            .self_parameter      = std::move(self_parameter),
            .name                = function_name,
            .return_type         = return_type,
            .function_type       = function_type,
        };
    }


    auto resolve_function_signature_only(
        Context                                            & context,
        Namespace                                          & space,
        hir::Function_signature                           && signature,
        tl::optional<std::vector<hir::Template_parameter>>&& hir_template_parameters,
        Allow_generalization                           const allow_generalization) -> utl::Pair<Scope, mir::Function::Signature>
    {
        auto [template_parameter_scope, mir_template_parameters] = std::invoke([&] {
            if (hir_template_parameters.has_value())
                return context.resolve_template_parameters(*hir_template_parameters, space);
            else
                return utl::Pair { Scope {}, std::vector<mir::Template_parameter> {} };
        });

        auto self_parameter =
            resolve_self_parameter(context, template_parameter_scope, signature.self_parameter);

        auto [signature_scope, function_parameters] =
            resolve_function_parameters(context, allow_generalization, signature.parameters, mir_template_parameters, std::move(template_parameter_scope), space);

        mir::Type const return_type = std::invoke([&] {
            if (signature.return_type.has_value())
                return context.resolve_type(*signature.return_type, signature_scope, space);
            else
                return context.unit_type(signature.name.source_view); // to be overwritten
        });

        return {
            std::move(signature_scope),
            make_function_signature(
                context,
                signature.name,
                return_type,
                std::move(self_parameter),
                std::move(function_parameters),
                std::move(mir_template_parameters)),
        };
    }


    auto resolve_function_signature_impl(
        Context                              & context,
        Function_info                        & function_info,
        hir::definition::Function           && function,
        std::vector<hir::Template_parameter>&& hir_template_parameters) -> void
    {
        Definition_state_guard const state_guard { context, function_info.state, function.signature.name };
        bool const has_explicit_return_type = function.signature.return_type.has_value();
        auto const name = function.signature.name;

        auto [signature_scope, signature] =
            resolve_function_signature_only(context, *function_info.home_namespace, std::move(function.signature), std::move(hir_template_parameters), Allow_generalization::yes);

        if (has_explicit_return_type) {
            context.generalize_to(signature.return_type, signature.template_parameters);
            function_info.value = Partially_resolved_function {
                .resolved_signature = std::move(signature),
                .signature_scope    = std::move(signature_scope),
                .unresolved_body    = std::move(function.body),
                .name               = name,
            };
        }
        else {
            mir::Expression function_body = context.resolve_expression(function.body, signature_scope, *function_info.home_namespace);
            signature.return_type = function_body.type;
            context.generalize_to(signature.return_type, signature.template_parameters);

            signature_scope.warn_about_unused_bindings(context);

            function_info.value = mir::Function {
                .signature = std::move(signature),
                .body      = std::move(function_body),
            };
        }
    }


    auto resolve_function_impl(
        Partially_resolved_function& function,
        Context                    & context,
        utl::Wrapper<Namespace>      home_namespace) -> mir::Function
    {
        mir::Expression body = context.resolve_expression(function.unresolved_body, function.signature_scope, *home_namespace);
        function.signature_scope.warn_about_unused_bindings(context);

        context.solve(constraint::Type_equality {
            .constrainer_type = function.resolved_signature.return_type,
            .constrained_type = body.type,
            .constrainer_note = constraint::Explanation {
                function.resolved_signature.return_type.source_view(),
                "The return type is specified to be {0}"
            },
            .constrained_note {
                body.type.source_view(),
                "But the body is of type {1}"
            }
        });

        return mir::Function {
            .signature = std::move(function.resolved_signature),
            .body      = std::move(body),
        };
    }


    auto resolve_struct_impl(
        hir::definition::Struct& structure,
        Context                & context,
        Scope                    scope,
        utl::Wrapper<Namespace>  home_namespace) -> mir::Struct
    {
        mir::Struct mir_structure {
            .members              = utl::vector_with_capacity(structure.members.size()),
            .name                 = structure.name,
            .associated_namespace = context.wrap(Namespace { .parent = home_namespace })
        };

        for (hir::definition::Struct::Member& member : structure.members) {
            mir::Type const member_type = context.resolve_type(member.type, scope, *home_namespace);
            context.ensure_non_generalizable(member_type, "A struct member");
            mir_structure.members.push_back({
                .name      = member.name,
                .type      = member_type,
                .is_public = member.is_public,
            });
        }

        return mir_structure;
    }


    auto resolve_enum_impl(
        hir::definition::Enum & enumeration,
        Context               & context,
        Scope                   scope,
        utl::Wrapper<Namespace> home_namespace,
        mir::Type               enumeration_type) -> mir::Enum
    {
        mir::Enum mir_enumeration {
            .constructors         = utl::vector_with_capacity(enumeration.constructors.size()),
            .name                 = enumeration.name,
            .associated_namespace = context.wrap(Namespace { .parent = home_namespace })
        };

        Scope constructor_scope = scope.make_child();

        for (hir::definition::Enum::Constructor& hir_constructor : enumeration.constructors) {
            mir::Enum_constructor const constructor = std::invoke([&] {
                if (hir_constructor.payload_type.has_value()) {
                    mir::Type payload_type = context.resolve_type(*hir_constructor.payload_type, constructor_scope, *home_namespace);
                    std::vector<mir::Type> constructor_function_parameter_types;

                    if (auto* const tuple = std::get_if<mir::type::Tuple>(&*payload_type.flattened_value())) {
                        constructor_function_parameter_types = tuple->field_types;
                    }
                    else {
                        constructor_function_parameter_types.reserve(1); // Try to avoid overallocating
                        constructor_function_parameter_types.push_back(payload_type);
                    }

                    mir::Type const function_type {
                        context.wrap_type(mir::type::Function {
                            .parameter_types = std::move(constructor_function_parameter_types),
                            .return_type     = enumeration_type,
                        }),
                        hir_constructor.source_view,
                    };
                    context.ensure_non_generalizable(function_type, "An enum constructor");

                    return mir::Enum_constructor {
                        .name          = hir_constructor.name,
                        .payload_type  = payload_type,
                        .function_type = function_type,
                        .enum_type     = enumeration_type,
                    };
                }
                else {
                    return mir::Enum_constructor {
                        .name      = hir_constructor.name,
                        .enum_type = enumeration_type
                    };
                }
            });

            mir_enumeration.constructors.push_back(constructor);
            mir_enumeration.associated_namespace->lower_table
                .add_new_or_abort(hir_constructor.name.identifier, Lower_variant { constructor });
        }

        return mir_enumeration;
    }

}


auto resolution::Context::resolve_function_signature(Function_info& info)
    -> mir::Function::Signature&
{
    if (auto* const function = std::get_if<hir::definition::Function>(&info.value))
        resolve_function_signature_impl(*this, info, std::move(*function), {});
    else if (auto* const function_template = std::get_if<hir::definition::Function_template>(&info.value))
        resolve_function_signature_impl(*this, info, std::move(function_template->definition), std::move(function_template->parameters));

    if (auto* const function = std::get_if<Partially_resolved_function>(&info.value))
        return function->resolved_signature;
    else
        return utl::get<mir::Function>(info.value).signature;
}


auto resolution::Context::resolve_function(utl::Wrapper<Function_info> const wrapped_info)
    -> mir::Function&
{
    Function_info& info = *wrapped_info;
    (void)resolve_function_signature(info);

    if (auto* const function = std::get_if<Partially_resolved_function>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, function->name  };
        info.value = resolve_function_impl(*function, *this, info.home_namespace);
    }
    
    return utl::get<mir::Function>(info.value);
}


auto resolution::Context::resolve_struct(utl::Wrapper<Struct_info> const wrapped_info) -> mir::Struct& {
    Struct_info& info = *wrapped_info;

    if (auto* const structure = std::get_if<hir::definition::Struct>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, structure->name };
        info.value = resolve_struct_impl(*structure, *this, Scope {}, info.home_namespace);
    }
    
    return utl::get<mir::Struct>(info.value);
}


auto resolution::Context::resolve_enum(utl::Wrapper<Enum_info> const wrapped_info) -> mir::Enum& {
    Enum_info& info = *wrapped_info;

    if (auto* const enumeration = std::get_if<hir::definition::Enum>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, enumeration->name };
        info.value = resolve_enum_impl(*enumeration, *this, Scope {}, info.home_namespace, info.enumeration_type);
    }
    
    return utl::get<mir::Enum>(info.value);
}


auto resolution::Context::resolve_alias(utl::Wrapper<Alias_info> const wrapped_info) -> mir::Alias& {
    Alias_info& info = *wrapped_info;

    if (auto* const alias = std::get_if<hir::definition::Alias>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, alias->name };
        Scope scope;
        mir::Type const aliased_type = resolve_type(alias->type, scope, *info.home_namespace);
        ensure_non_generalizable(aliased_type, "An aliased type");
        info.value = mir::Alias { .aliased_type = aliased_type, .name = alias->name, };
    }

    return utl::get<mir::Alias>(info.value);
}


auto resolution::Context::resolve_typeclass(utl::Wrapper<Typeclass_info> const wrapped_info) -> mir::Typeclass& {
    Typeclass_info& info = *wrapped_info;

    if (auto* const hir_typeclass = std::get_if<hir::definition::Typeclass>(&info.value)) {
        utl::always_assert(hir_typeclass->type_signatures.empty() && hir_typeclass->type_template_signatures.empty());

        Definition_state_guard const state_guard { *this, info.state, hir_typeclass->name };
        Self_type_guard const self_type_guard { *this, self_placeholder_type(info.name.source_view) };

        mir::Typeclass mir_typeclass { .name = info.name };

        auto handle_signature = [&](hir::Function_signature&& signature, tl::optional<std::vector<hir::Template_parameter>>&& template_parameters) {
            // Should be guaranteed by the parser
            utl::always_assert(signature.return_type.has_value());

            auto [signature_scope, mir_signature] =
                resolve_function_signature_only(*this, *info.home_namespace, std::move(signature), std::move(template_parameters), Allow_generalization::no);
            signature_scope.warn_about_unused_bindings(*this);

            ensure_non_generalizable(mir_signature.function_type, "A class function signature");

            auto const identifier = mir_signature.name.identifier;
            mir_typeclass.function_signatures.add_new_or_abort(identifier, std::move(mir_signature));
        };

        for (hir::Function_signature& signature : hir_typeclass->function_signatures)
            handle_signature(std::move(signature), tl::nullopt);
        for (hir::Function_template_signature& signature : hir_typeclass->function_template_signatures)
            handle_signature(std::move(signature.function_signature), std::move(signature.template_parameters));

        info.value = std::move(mir_typeclass);
    }

    return utl::get<mir::Typeclass>(info.value);
}


auto resolution::Context::resolve_implementation(utl::Wrapper<Implementation_info> const wrapped_info) -> mir::Implementation& {
    Implementation_info& info = *wrapped_info;

    if (auto* const implementation = std::get_if<hir::definition::Implementation>(&info.value)) {
        // Definition_state_guard is not needed because an implementation block can not be referred to

        Scope scope;
        mir::Type const self_type = resolve_type(implementation->type, scope, *info.home_namespace);

        utl::Wrapper<Namespace> self_type_associated_namespace = std::invoke([&] {
            if (tl::optional space = associated_namespace_if(self_type)) return *space;
            error(self_type.source_view(), {
                .message           = "{} does not have an associated namespace, so it can not be the Self type in an implementation block",
                .message_arguments = fmt::make_format_args(self_type)
            });
        });

        Self_type_guard const self_type_guard { *this, self_type };
        mir::Implementation::Definitions definitions;

        for (hir::Definition& definition : implementation->definitions) {
            // TODO: deduplicate

            utl::match(definition.value,
                [&](hir::definition::Function& function) {
                    ast::Name const name = function.signature.name;
                    auto function_info = wrap(Function_info {
                        .value          = std::move(function),
                        .home_namespace = info.home_namespace,
                        .name           = name,
                    });
                    (void)resolve_function(function_info); // TODO: maybe avoid resolving here, but how to handle Self type tracking then?
                    add_to_namespace(*self_type_associated_namespace, name, function_info);
                    definitions.functions.add_new_or_abort(name.identifier, function_info);
                },
                [&](hir::definition::Function_template& function_template) {
                    ast::Name const name = function_template.definition.signature.name;
                    auto function_template_info = wrap(Function_info {
                        .value          = std::move(function_template),
                        .home_namespace = info.home_namespace,
                        .name           = name
                    });
                    (void)resolve_function(function_template_info);
                    add_to_namespace(*self_type_associated_namespace, name, function_template_info);
                    definitions.functions.add_new_or_abort(name.identifier, function_template_info);
                },
                [](auto const&) {
                    utl::todo();
                });
        }

        info.value = mir::Implementation {
            .definitions = std::move(definitions),
            .self_type = self_type
        };

    }

    return utl::get<mir::Implementation>(info.value);
}


auto resolution::Context::resolve_instantiation(utl::Wrapper<Instantiation_info> const wrapped_info) -> mir::Instantiation& {
    (void)wrapped_info;
    utl::todo();
}


auto resolution::Context::resolve_struct_template(utl::Wrapper<Struct_template_info> const wrapped_info)
    -> mir::Struct_template&
{
    Struct_template_info& info = *wrapped_info;

    if (auto* const structure = std::get_if<hir::definition::Struct_template>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, info.name };

        auto [scope, parameters] = // NOLINT
            resolve_template_parameters(structure->parameters, *info.home_namespace);

        info.value = mir::Struct_template {
            .definition = resolve_struct_impl(
                structure->definition,
                *this,
                std::move(scope),
                info.home_namespace),
            .parameters = std::move(parameters)
        };
    }
    return utl::get<mir::Struct_template>(info.value);
}


auto resolution::Context::resolve_enum_template(utl::Wrapper<Enum_template_info> const wrapped_info)
    -> mir::Enum_template&
{
    Enum_template_info& info = *wrapped_info;

    if (auto* const enumeration = std::get_if<hir::definition::Enum_template>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, info.name };

        auto [template_parameter_scope, template_parameters] =
            resolve_template_parameters(enumeration->parameters, *info.home_namespace);

        info.value = mir::Enum_template {
            .definition = resolve_enum_impl(
                enumeration->definition,
                *this,
                std::move(template_parameter_scope),
                info.home_namespace,
                info.parameterized_type_of_this),
            .parameters = std::move(template_parameters)
        };
    }
    
    return utl::get<mir::Enum_template>(info.value);
}


auto resolution::Context::resolve_alias_template(utl::Wrapper<Alias_template_info> const wrapped_info)
    -> mir::Alias_template&
{
    Alias_template_info& info = *wrapped_info;

    if (auto* const alias_template = std::get_if<hir::definition::Alias_template>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, info.name };

        auto [template_parameter_scope, template_parameters] =
            resolve_template_parameters(alias_template->parameters, *info.home_namespace);

        info.value = mir::Alias_template {
            .definition = mir::Alias {
                .aliased_type = resolve_type(alias_template->definition.type, template_parameter_scope, *info.home_namespace),
                .name         = info.name
            },
            .parameters = std::move(template_parameters)
        };
    }

    return utl::get<mir::Alias_template>(info.value);
}


auto resolution::Context::resolve_typeclass_template(utl::Wrapper<Typeclass_template_info>)
    -> mir::Typeclass_template&
{
    utl::todo();
}


auto resolution::Context::resolve_implementation_template(utl::Wrapper<Implementation_template_info>) -> mir::Implementation_template& {
    utl::todo();
}


auto resolution::Context::resolve_instantiation_template(utl::Wrapper<Instantiation_template_info>) -> mir::Instantiation_template& {
    utl::todo();
}