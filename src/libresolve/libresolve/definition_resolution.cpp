#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {

    // Prevents unresolvable circular dependencies
    class Definition_state_guard {
        Definition_state& definition_state;
        int               initial_exception_count;
    public:
        Definition_state_guard(
            Context& context, Definition_state& state, compiler::Name_dynamic const name)
            : definition_state { state }
            , initial_exception_count { std::uncaught_exceptions() }
        {
            if (state == Definition_state::currently_on_resolution_stack) {
                context.error(name.source_view, { "Unable to resolve circular dependency" });
            }
            else {
                state = Definition_state::currently_on_resolution_stack;
            }
        }

        ~Definition_state_guard()
        {
            // If the destructor is called due to an uncaught exception
            // in definition resolution code, don't modify the state.
            if (std::uncaught_exceptions() == initial_exception_count) {
                definition_state = Definition_state::resolved;
            }
        }
    };

    // Sets and resets the Self type within classes and impl/inst blocks
    class Self_type_guard {
        tl::optional<hir::Type>& current_self_type;
        tl::optional<hir::Type>  previous_self_type;
    public:
        Self_type_guard(Context& context, hir::Type new_self_type)
            : current_self_type { context.current_self_type }
            , previous_self_type { current_self_type }
        {
            current_self_type = new_self_type;
        }

        ~Self_type_guard()
        {
            current_self_type = previous_self_type;
        }
    };

    enum class Allow_generalization { yes, no };

    auto resolve_function_parameters(
        Context&                                 context,
        Allow_generalization const               allow_generalization,
        std::span<ast::Function_parameter> const ast_parameters,
        std::vector<hir::Template_parameter>&    template_parameters,
        Scope                                    signature_scope,
        Namespace& home_namespace) -> utl::Pair<Scope, std::vector<hir::Function_parameter>>
    {
        std::vector<hir::Function_parameter> hir_parameters;
        hir_parameters.reserve(ast_parameters.size());

        for (ast::Function_parameter& parameter : ast_parameters) {
            if (!parameter.type.has_value()) {
                context.error(
                    parameter.pattern->source_view,
                    { "Implicit parameter types are not supported yet" });
            }
            if (parameter.default_argument.has_value()) {
                context.error(
                    (*parameter.default_argument)->source_view,
                    { "Default arguments are not supported yet" });
            }

            hir::Type const parameter_type
                = context.resolve_type(*utl::get(parameter.type), signature_scope, home_namespace);
            hir::Pattern parameter_pattern = context.resolve_pattern(
                *parameter.pattern, parameter_type, signature_scope, home_namespace);

            if (!parameter_pattern.is_exhaustive_by_itself) {
                context.error(
                    parameter_pattern.source_view, { "Inexhaustive function parameter pattern" });
            }

            if (allow_generalization == Allow_generalization::yes) {
                context.generalize_to(parameter_type, template_parameters);
            }

            hir_parameters.push_back(hir::Function_parameter {
                .pattern = std::move(parameter_pattern),
                .type    = parameter_type,
            });
        }

        return { std::move(signature_scope), std::move(hir_parameters) };
    }

    auto resolve_self_parameter(
        Context& context, Scope& scope, tl::optional<ast::Self_parameter> const& self)
        -> tl::optional<hir::Self_parameter>
    {
        return self.transform([&](ast::Self_parameter const& self) {
            return hir::Self_parameter {
                .mutability   = context.resolve_mutability(self.mutability, scope),
                .is_reference = self.is_reference,
                .source_view  = self.source_view,
            };
        });
    }

    auto make_function_signature(
        Context&                               context,
        compiler::Name_lower const             function_name,
        hir::Type const                        return_type,
        tl::optional<hir::Self_parameter>&&    self_parameter,
        std::vector<hir::Function_parameter>&& function_parameters,
        std::vector<hir::Template_parameter>&& template_parameters) -> hir::Function::Signature
    {
        hir::Type const function_type {
            context.wrap_type(hir::type::Function {
                .parameter_types = utl::map(&hir::Function_parameter::type, function_parameters),
                .return_type     = return_type,
            }),
            function_name.source_view,
        };
        return hir::Function::Signature {
            .template_parameters = std::move(template_parameters),
            .parameters          = std::move(function_parameters),
            .self_parameter      = std::move(self_parameter),
            .name                = function_name,
            .return_type         = return_type,
            .function_type       = function_type,
        };
    }

    auto resolve_function_signature_only(
        Context&                   context,
        Namespace&                 space,
        ast::Function_signature&&  signature,
        Allow_generalization const allow_generalization)
        -> utl::Pair<Scope, hir::Function::Signature>
    {
        auto [template_parameter_scope, hir_template_parameters] = std::invoke([&] {
            if (signature.template_parameters.empty()) {
                return utl::Pair { Scope {}, std::vector<hir::Template_parameter> {} };
            }
            else {
                return context.resolve_template_parameters(signature.template_parameters, space);
            }
        });

        auto self_parameter
            = resolve_self_parameter(context, template_parameter_scope, signature.self_parameter);

        auto [signature_scope, function_parameters] = // NOLINT: field order
            resolve_function_parameters(
                context,
                allow_generalization,
                signature.function_parameters,
                hir_template_parameters,
                std::move(template_parameter_scope),
                space);

        hir::Type const return_type = std::invoke([&] {
            if (signature.return_type.has_value()) {
                return context.resolve_type(*signature.return_type, signature_scope, space);
            }
            else {
                return context.unit_type(signature.name.source_view); // to be overwritten
            }
        });

        return {
            std::move(signature_scope),
            make_function_signature(
                context,
                signature.name,
                return_type,
                std::move(self_parameter),
                std::move(function_parameters),
                std::move(hir_template_parameters)),
        };
    }

    auto resolve_function_signature_impl(
        Context& context, Function_info& function_info, ast::definition::Function&& function)
        -> void
    {
        Definition_state_guard const state_guard { context,
                                                   function_info.state,
                                                   function.signature.name.as_dynamic() };
        bool const has_explicit_return_type = function.signature.return_type.has_value();
        auto const name                     = function.signature.name;

        auto [signature_scope, signature] = resolve_function_signature_only(
            context,
            *function_info.home_namespace,
            std::move(function.signature),
            Allow_generalization::yes);

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
            hir::Expression function_body = context.resolve_expression(
                function.body, signature_scope, *function_info.home_namespace);
            signature.return_type = function_body.type;
            context.generalize_to(signature.return_type, signature.template_parameters);

            signature_scope.warn_about_unused_bindings(context);

            function_info.value = hir::Function {
                .signature = std::move(signature),
                .body      = std::move(function_body),
            };
        }
    }

    auto resolve_function_impl(
        Partially_resolved_function& function,
        Context&                     context,
        utl::Wrapper<Namespace>      home_namespace) -> hir::Function
    {
        hir::Expression body = context.resolve_expression(
            function.unresolved_body, function.signature_scope, *home_namespace);
        function.signature_scope.warn_about_unused_bindings(context);

        context.solve(constraint::Type_equality {
            .constrainer_type = function.resolved_signature.return_type,
            .constrained_type = body.type,
            .constrainer_note = constraint::Explanation {
                function.resolved_signature.return_type.source_view(),
                "The return type is specified to be {0}",
            },
            .constrained_note {
                body.type.source_view(),
                "But the body is of type {1}",
            }
        });

        return hir::Function {
            .signature = std::move(function.resolved_signature),
            .body      = std::move(body),
        };
    }

    auto resolve_struct_impl(
        ast::definition::Struct& structure,
        Context&                 context,
        Scope                    scope,
        utl::Wrapper<Namespace>  home_namespace) -> hir::Struct
    {
        hir::Struct hir_structure { .members = utl::vector_with_capacity(structure.members.size()),
                                    .name    = structure.name,
                                    .associated_namespace
                                    = context.wrap(Namespace { .parent = home_namespace }) };

        for (ast::definition::Struct::Member& member : structure.members) {
            hir::Type const member_type = context.resolve_type(member.type, scope, *home_namespace);
            context.ensure_non_generalizable(member_type, "A struct member");
            hir_structure.members.push_back({
                .name      = member.name,
                .type      = member_type,
                .is_public = member.is_public,
            });
        }

        return hir_structure;
    }

    auto resolve_enum_constructor(
        ast::definition::Enum::Constructor& constructor,
        Context&                            context,
        Scope&                              scope,
        utl::Wrapper<Namespace> const       home_namespace,
        hir::Type const                     enumeration_type) -> hir::Enum_constructor
    {
        if (!constructor.payload_types.has_value()) {
            return hir::Enum_constructor {
                .name      = constructor.name,
                .enum_type = enumeration_type,
            };
        }

        auto const resolve_type = [&](utl::Wrapper<ast::Type> const type) -> hir::Type {
            return context.resolve_type(*type, scope, *home_namespace);
        };
        auto payload_types = utl::map(resolve_type, *constructor.payload_types);

        hir::Type const payload_type = std::invoke([&] {
            static_assert(std::is_trivially_copyable_v<hir::Type>);
            if (payload_types.size() == 1) {
                return payload_types.front();
            }
            auto const source_view = payload_types.front().source_view().combine_with(
                payload_types.back().source_view());
            return hir::Type { context.wrap_type(hir::type::Tuple { payload_types }), source_view };
        });
        hir::Type const function_type {
            context.wrap_type(hir::type::Function {
                .parameter_types = std::move(payload_types),
                .return_type     = enumeration_type,
            }),
            constructor.source_view,
        };

        context.ensure_non_generalizable(payload_type, "An enum constructor");
        return hir::Enum_constructor {
            .name          = constructor.name,
            .payload_type  = payload_type,
            .function_type = function_type,
            .enum_type     = enumeration_type,
        };
    }

    auto resolve_enum_impl(
        ast::definition::Enum&  enumeration,
        Context&                context,
        Scope                   scope,
        utl::Wrapper<Namespace> home_namespace,
        hir::Type               enumeration_type) -> hir::Enum
    {
        hir::Enum mir_enumeration {
            .constructors         = utl::vector_with_capacity(enumeration.constructors.size()),
            .name                 = enumeration.name,
            .associated_namespace = context.wrap(Namespace { .parent = home_namespace }),
        };

        Scope constructor_scope = scope.make_child();

        for (ast::definition::Enum::Constructor& ast_constructor : enumeration.constructors) {
            hir::Enum_constructor const hir_constructor = resolve_enum_constructor(
                ast_constructor, context, scope, home_namespace, enumeration_type);
            mir_enumeration.constructors.push_back(hir_constructor);
            mir_enumeration.associated_namespace->lower_table.add_new_or_abort(
                hir_constructor.name.identifier, Lower_variant { hir_constructor });
        }

        return mir_enumeration;
    }

} // namespace

auto libresolve::Context::resolve_function_signature(Function_info& info)
    -> hir::Function::Signature&
{
    if (auto* const function = std::get_if<ast::definition::Function>(&info.value)) {
        resolve_function_signature_impl(*this, info, std::move(*function));
    }

    if (auto* const function = std::get_if<Partially_resolved_function>(&info.value)) {
        return function->resolved_signature;
    }
    else {
        return utl::get<hir::Function>(info.value).signature;
    }
}

auto libresolve::Context::resolve_function(utl::Wrapper<Function_info> const wrapped_info)
    -> hir::Function&
{
    Function_info& info = *wrapped_info;
    (void)resolve_function_signature(info);

    if (auto* const function = std::get_if<Partially_resolved_function>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, function->name.as_dynamic() };
        info.value = resolve_function_impl(*function, *this, info.home_namespace);
    }

    return utl::get<hir::Function>(info.value);
}

auto libresolve::Context::resolve_struct(utl::Wrapper<Struct_info> const wrapped_info)
    -> hir::Struct&
{
    Struct_info& info = *wrapped_info;

    if (auto* const structure = std::get_if<ast::definition::Struct>(&info.value)) {
        Definition_state_guard const state_guard { *this,
                                                   info.state,
                                                   structure->name.as_dynamic() };
        info.value = resolve_struct_impl(*structure, *this, Scope {}, info.home_namespace);
    }

    return utl::get<hir::Struct>(info.value);
}

auto libresolve::Context::resolve_enum(utl::Wrapper<Enum_info> const wrapped_info) -> hir::Enum&
{
    Enum_info& info = *wrapped_info;

    if (auto* const enumeration = std::get_if<ast::definition::Enum>(&info.value)) {
        Definition_state_guard const state_guard { *this,
                                                   info.state,
                                                   enumeration->name.as_dynamic() };
        info.value = resolve_enum_impl(
            *enumeration, *this, Scope {}, info.home_namespace, info.enumeration_type);
    }

    return utl::get<hir::Enum>(info.value);
}

auto libresolve::Context::resolve_alias(utl::Wrapper<Alias_info> const wrapped_info) -> hir::Alias&
{
    Alias_info& info = *wrapped_info;

    if (auto* const alias = std::get_if<ast::definition::Alias>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, alias->name.as_dynamic() };
        Scope                        scope;
        hir::Type const aliased_type = resolve_type(alias->type, scope, *info.home_namespace);
        ensure_non_generalizable(aliased_type, "An aliased type");
        info.value = hir::Alias {
            .name         = alias->name,
            .aliased_type = aliased_type,
        };
    }

    return utl::get<hir::Alias>(info.value);
}

auto libresolve::Context::resolve_typeclass(utl::Wrapper<Typeclass_info> const wrapped_info)
    -> hir::Typeclass&
{
    Typeclass_info& info = *wrapped_info;

    if (auto* const ast_typeclass = std::get_if<ast::definition::Typeclass>(&info.value)) {
        utl::always_assert(ast_typeclass->type_signatures.empty());

        Definition_state_guard const state_guard { *this,
                                                   info.state,
                                                   ast_typeclass->name.as_dynamic() };
        Self_type_guard const        self_type_guard { *this,
                                                self_placeholder_type(info.name.source_view) };

        hir::Typeclass hir_typeclass { .name = info.name };

        auto handle_signature = [&](ast::Function_signature&& signature) {
            // Should be guaranteed by the parser
            utl::always_assert(signature.return_type.has_value());

            auto [signature_scope, hir_signature] = // NOLINT: field order
                resolve_function_signature_only(
                    *this, *info.home_namespace, std::move(signature), Allow_generalization::no);
            signature_scope.warn_about_unused_bindings(*this);

            ensure_non_generalizable(hir_signature.function_type, "A class function signature");

            auto const identifier = hir_signature.name.identifier;
            hir_typeclass.function_signatures.add_new_or_abort(
                identifier, std::move(hir_signature));
        };

        for (ast::Function_signature& signature : ast_typeclass->function_signatures) {
            handle_signature(std::move(signature));
        }

        info.value = std::move(hir_typeclass);
    }

    return utl::get<hir::Typeclass>(info.value);
}

auto libresolve::Context::resolve_implementation(
    utl::Wrapper<Implementation_info> const wrapped_info) -> hir::Implementation&
{
    Implementation_info& info = *wrapped_info;

    if (auto* const implementation = std::get_if<ast::definition::Implementation>(&info.value)) {
        // Definition_state_guard is not needed because an implementation block can not be referred
        // to

        Scope           scope;
        hir::Type const self_type = resolve_type(implementation->type, scope, *info.home_namespace);

        utl::Wrapper<Namespace> self_type_associated_namespace = std::invoke([&] {
            if (tl::optional space = associated_namespace_if(self_type)) {
                return *space;
            }
            error(
                self_type.source_view(),
                {
                    .message = std::format(
                        "{} does not have an associated namespace, so it can not be the Self type "
                        "in an implementation block",
                        hir::to_string(self_type)),
                });
        });

        Self_type_guard const            self_type_guard { *this, self_type };
        hir::Implementation::Definitions definitions;

        for (ast::Definition& definition : implementation->definitions) {
            // TODO: deduplicate

            utl::match(
                definition.value,
                [&](ast::definition::Function& function) {
                    auto const name          = function.signature.name;
                    auto       function_info = wrap(Function_info {
                              .value          = std::move(function),
                              .home_namespace = info.home_namespace,
                              .name           = name,
                    });
                    (void)resolve_function(function_info); // TODO: maybe avoid resolving here, but
                                                           // how to handle Self type tracking then?
                    add_to_namespace(*self_type_associated_namespace, name, function_info);
                    definitions.functions.add_new_or_abort(name.identifier, function_info);
                },
                [](auto const&) { utl::todo(); });
        }

        info.value = hir::Implementation {
            .definitions = std::move(definitions),
            .self_type   = self_type,
        };
    }

    return utl::get<hir::Implementation>(info.value);
}

auto libresolve::Context::resolve_instantiation(utl::Wrapper<Instantiation_info> const wrapped_info)
    -> hir::Instantiation&
{
    (void)wrapped_info;
    utl::todo();
}

auto libresolve::Context::resolve_struct_template(
    utl::Wrapper<Struct_template_info> const wrapped_info) -> hir::Struct_template&
{
    Struct_template_info& info = *wrapped_info;

    if (auto* const structure = std::get_if<ast::definition::Struct_template>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, info.name.as_dynamic() };

        auto [scope, parameters] = // NOLINT
            resolve_template_parameters(structure->parameters, *info.home_namespace);

        info.value = hir::Struct_template {
            .definition = resolve_struct_impl(
                structure->definition, *this, std::move(scope), info.home_namespace),
            .parameters = std::move(parameters),
        };
    }
    return utl::get<hir::Struct_template>(info.value);
}

auto libresolve::Context::resolve_enum_template(utl::Wrapper<Enum_template_info> const wrapped_info)
    -> hir::Enum_template&
{
    Enum_template_info& info = *wrapped_info;

    if (auto* const enumeration = std::get_if<ast::definition::Enum_template>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, info.name.as_dynamic() };

        auto [template_parameter_scope, template_parameters]
            = resolve_template_parameters(enumeration->parameters, *info.home_namespace);

        info.value = hir::Enum_template {
            .definition = resolve_enum_impl(
                enumeration->definition,
                *this,
                std::move(template_parameter_scope),
                info.home_namespace,
                info.parameterized_type_of_this),
            .parameters = std::move(template_parameters),
        };
    }

    return utl::get<hir::Enum_template>(info.value);
}

auto libresolve::Context::resolve_alias_template(
    utl::Wrapper<Alias_template_info> const wrapped_info) -> hir::Alias_template&
{
    Alias_template_info& info = *wrapped_info;

    if (auto* const alias_template = std::get_if<ast::definition::Alias_template>(&info.value)) {
        Definition_state_guard const state_guard { *this, info.state, info.name.as_dynamic() };

        auto [template_parameter_scope, template_parameters]
            = resolve_template_parameters(alias_template->parameters, *info.home_namespace);

        info.value = hir::Alias_template {
            .definition = hir::Alias {
                .name         = info.name,
                .aliased_type = resolve_type(alias_template->definition.type, template_parameter_scope, *info.home_namespace),
            },
            .parameters = std::move(template_parameters),
        };
    }

    return utl::get<hir::Alias_template>(info.value);
}

auto libresolve::Context::resolve_typeclass_template(utl::Wrapper<Typeclass_template_info>)
    -> hir::Typeclass_template&
{
    utl::todo();
}

auto libresolve::Context::resolve_implementation_template(
    utl::Wrapper<Implementation_template_info>) -> hir::Implementation_template&
{
    utl::todo();
}

auto libresolve::Context::resolve_instantiation_template(utl::Wrapper<Instantiation_template_info>)
    -> hir::Instantiation_template&
{
    utl::todo();
}
