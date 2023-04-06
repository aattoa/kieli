#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    using namespace resolution;


    struct [[nodiscard]] Substitutions {
        utl::Flatmap<mir::Template_parameter_tag, mir::Type>       type_substitutions;
        utl::Flatmap<mir::Template_parameter_tag, mir::Mutability> mutability_substitutions;

        auto add_substitution(mir::Template_parameter const& parameter, mir::Template_argument const& argument) -> void {
            std::visit(utl::Overload {
                [&](mir::Template_parameter::Type_parameter const&, mir::Type const type_argument) {
                    type_substitutions.add_new_or_abort(parameter.reference_tag, type_argument);
                },
                [&](mir::Template_parameter::Mutability_parameter, mir::Mutability const mutability_argument) {
                    mutability_substitutions.add_new_or_abort(parameter.reference_tag, mutability_argument);
                },
                [&](auto const&, auto const&) {
                    utl::abort(); // Should be unreachable because resolve_template_arguments handles parameter/argument mismatches
                }
            }, parameter.value, argument.value);
        }

        Substitutions(
            std::span<mir::Template_parameter const> const parameters,
            std::span<mir::Template_argument  const> const arguments)
        {
            utl::always_assert(parameters.size() == arguments.size()); // Should be guaranteed by resolve_template_arguments
            for (auto [parameter, argument] : ranges::views::zip(parameters, arguments))
                add_substitution(parameter, argument);
        }
    };


    struct [[nodiscard]] Substitution_context;

    auto instantiate(mir::Expression        const&, Substitution_context) -> mir::Expression;
    auto instantiate(mir::Type              const&, Substitution_context) -> mir::Type;
    auto instantiate(mir::Pattern           const&, Substitution_context) -> mir::Pattern;
    auto instantiate(mir::Mutability        const&, Substitution_context) -> mir::Mutability;
    auto instantiate(mir::Template_argument const&, Substitution_context) -> mir::Template_argument;
    auto instantiate(mir::Self_parameter    const&, Substitution_context) -> mir::Self_parameter;

    template <class Node>
    concept instantiable = requires (Node const node, Substitution_context const context) {
        { instantiate(node, context) } -> std::same_as<Node>;
    };

    struct Substitution_context {
        Substitutions& substitutions;
        Context      & resolution_context;
        Scope        & scope;
        Namespace    & space;

        [[nodiscard]]
        auto recurse(instantiable auto const& node) const {
            return instantiate(node, *this);
        }
        template <instantiable T>
        auto recurse(utl::Wrapper<T> const wrapper) const -> utl::Wrapper<T> {
            return utl::wrap(recurse(*wrapper));
        }
        [[nodiscard]]
        auto recurse() const noexcept {
            return [*this](auto const& node)
                requires requires (Substitution_context context) { context.recurse(node); }
            {
                return recurse(node);
            };
        }
    };


    auto resolve_template_arguments(
        Context                                      & context,
        std::span<mir::Template_parameter const> const parameters,
        std::span<hir::Template_argument  const> const arguments,
        utl::Source_view                          const instantiation_view,
        Scope                                        & scope,
        Namespace                                    & space) -> std::vector<mir::Template_argument>
    {
        auto const first_defaulted = ranges::find_if(parameters, [](auto const& parameter) { return parameter.default_argument.has_value(); });
        utl::Usize const required_arguments = utl::unsigned_distance(parameters.begin(), first_defaulted);

        {
            std::string_view const was_or_were = arguments.size() == 1 ? "was" : "were";

            if (required_arguments != parameters.size()) { // Default arguments
                if (arguments.size() < required_arguments) {
                    context.error(instantiation_view, {
                        .message           = "The template requires at least {} arguments, but {} {} supplied",
                        .message_arguments = fmt::make_format_args(required_arguments, arguments.size(), was_or_were)
                    });
                }
                else if (arguments.size() > parameters.size()) {
                    context.error(instantiation_view, {
                        .message           = "The template has {} parameters, but {} {} supplied",
                        .message_arguments = fmt::make_format_args(parameters.size(), arguments.size(), was_or_were)
                    });
                }
            }
            else { // No default arguments
                if (parameters.size() != arguments.size()) {
                    context.error(instantiation_view, {
                        .message = "The template requires {} arguments, but {} {} supplied",
                        .message_arguments = fmt::make_format_args(required_arguments, arguments.size(), was_or_were)
                    });
                }
            }
        }

        auto mir_arguments = utl::vector_with_capacity<mir::Template_argument>(parameters.size());

        utl::Usize i = 0;

        // Handle explicit arguments
        for (; i != arguments.size(); ++i) {
            mir::Template_parameter const& parameter = parameters[i];
            hir::Template_argument const& argument = arguments[i];

            std::visit(utl::Overload {
                [&](mir::Template_parameter::Type_parameter const& type_parameter, utl::Wrapper<hir::Type> const type_argument) {
                    if (!type_parameter.classes.empty()) { utl::todo(); }
                    mir_arguments.emplace_back(context.resolve_type(*type_argument, scope, space));
                },
                [&](mir::Template_parameter::Type_parameter const& type_parameter, hir::Template_argument::Wildcard const wildcard) {
                    if (!type_parameter.classes.empty()) { utl::todo(); }
                    mir_arguments.emplace_back(context.fresh_general_unification_type_variable(wildcard.source_view));
                },
                [&](mir::Template_parameter::Mutability_parameter, ast::Mutability const mutability_argument) {
                    mir_arguments.emplace_back(context.resolve_mutability(mutability_argument, scope));
                },
                [&](mir::Template_parameter::Mutability_parameter, hir::Template_argument::Wildcard const wildcard) {
                    mir_arguments.emplace_back(context.fresh_unification_mutability_variable(wildcard.source_view));
                },
                [&](auto const&, auto const&) {
                    context.error(instantiation_view, {
                        // TODO: better message
                        .message           = "Argument {} is incompatible with parameter {}",
                        .message_arguments = fmt::make_format_args(argument, parameter)
                    });
                }
            }, parameter.value, argument.value);
        }

        Substitutions default_argument_substitutions { parameters.subspan(0, arguments.size()), mir_arguments };
        Substitution_context default_argument_substitution_context { default_argument_substitutions, context, scope, space };

        // Handle default arguments
        for (; i != parameters.size(); ++i) {
            mir::Template_argument default_argument = instantiate(utl::get(parameters[i].default_argument), default_argument_substitution_context);
            default_argument_substitutions.add_substitution(parameters[i], default_argument);
            mir_arguments.push_back(std::move(default_argument));
        }

        return mir_arguments;
    }


    auto instantiate_function_template_application(
        Context                                 & resolution_context,
        mir::Function_template                  & function_template,
        utl::Wrapper<Function_template_info> const template_info,
        std::vector<mir::Template_argument>    && template_arguments,
        Scope                                   & scope,
        Namespace                               & space) -> utl::Wrapper<Function_info>
    {
        Substitutions substitutions { function_template.parameters, template_arguments };

        Substitution_context substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space
        };

        auto const instantiate_parameter = [=](mir::Function_parameter const& parameter) {
            return mir::Function_parameter {
                .pattern = instantiate(parameter.pattern, substitution_context),
                .type    = instantiate(parameter.type,    substitution_context)
            };
        };

        tl::optional<mir::Self_parameter> concrete_self_parameter =
            function_template.definition.self_parameter.transform(substitution_context.recurse());

        std::vector<mir::Function_parameter> concrete_function_parameters =
            utl::map(instantiate_parameter, function_template.definition.signature.parameters);

        mir::Type const concrete_return_type =
            instantiate(function_template.definition.signature.return_type, substitution_context);

        mir::Type const concrete_function_type {
            .value = wrap_type(mir::type::Function {
                .parameter_types = utl::map(&mir::Function_parameter::type, concrete_function_parameters),
                .return_type     = concrete_return_type
            }),
            .source_view = template_info->name.source_view
        };

        mir::Function concrete_function {
            .signature {
                .parameters    = std::move(concrete_function_parameters),
                .return_type   = concrete_return_type,
                .function_type = concrete_function_type
            },
            .body           = instantiate(function_template.definition.body, substitution_context),
            .name           = function_template.definition.name,
            .self_parameter = std::move(concrete_self_parameter)
        };

        auto const info = utl::wrap(Function_info {
            .value          = std::move(concrete_function),
            .home_namespace = template_info->home_namespace,
            .state          = Definition_state::resolved,
            .name           = template_info->name,
            .template_instantiation_info = Template_instantiation_info<Function_template_info> {
                template_info,
                function_template.parameters,
                std::move(template_arguments)
            }
        });
        function_template.instantiations.push_back(info);
        return info;
    }


    auto instantiate_struct_template_application(
        Context                               & resolution_context,
        mir::Struct_template                  & struct_template,
        utl::Wrapper<Struct_template_info> const template_info,
        std::vector<mir::Template_argument>  && template_arguments,
        Scope                                 & scope,
        Namespace                             & space) -> utl::Wrapper<Struct_info>
    {
        Substitutions substitutions { struct_template.parameters, template_arguments };

        Substitution_context substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space
        };

        mir::Struct concrete_struct {
            .members              = utl::vector_with_capacity(struct_template.definition.members.size()),
            .name                 = template_info->name,
            .associated_namespace = utl::wrap(Namespace {})
        };

        for (mir::Struct::Member const& member : struct_template.definition.members) {
            concrete_struct.members.push_back(
                mir::Struct::Member {
                    .name      = member.name,
                    .type      = instantiate(member.type, substitution_context),
                    .is_public = member.is_public,
                }
            );
        }

        mir::Type concrete_type =
            resolution_context.temporary_placeholder_type(concrete_struct.name.source_view);

        auto const concrete_info = utl::wrap(Struct_info {
            .value          = std::move(concrete_struct),
            .home_namespace = template_info->home_namespace,
            .structure_type = concrete_type,
            .state          = Definition_state::resolved,
            .name           = template_info->name,
            .template_instantiation_info = Template_instantiation_info<Struct_template_info> {
                template_info,
                struct_template.parameters,
                std::move(template_arguments)
            }
        });
        *concrete_type.value = mir::type::Structure {
            .info           = concrete_info,
            .is_application = true
        };

        struct_template.instantiations.push_back(concrete_info);
        return concrete_info;
    }


    auto instantiate_enum_template_application(
        Context                              & resolution_context,
        mir::Enum_template                   & enum_template,
        utl::Wrapper<Enum_template_info> const template_info,
        std::vector<mir::Template_argument> && template_arguments,
        Scope                                & scope,
        Namespace                            & space) -> utl::Wrapper<Enum_info>
    {
        Substitutions substitutions { enum_template.parameters, template_arguments };

        Substitution_context substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space,
        };

        mir::Enum concrete_enum {
            .constructors         = utl::vector_with_capacity(enum_template.definition.constructors.size()),
            .name                 = template_info->name,
            .associated_namespace = utl::wrap(Namespace { .parent = template_info->home_namespace })
        };

        mir::Type concrete_type =
            resolution_context.temporary_placeholder_type(concrete_enum.name.source_view);


        for (mir::Enum_constructor& constructor : enum_template.definition.constructors) {
            mir::Enum_constructor concrete_constructor {
                .name          = constructor.name,
                .payload_type  = constructor.payload_type.transform(substitution_context.recurse()),
                .function_type = constructor.function_type.transform([&](mir::Type const function_type) {
                    return mir::Type {
                        .value = wrap_type(mir::type::Function {
                            .parameter_types = utl::map(substitution_context.recurse(), utl::get<mir::type::Function>(*function_type.value).parameter_types),
                            .return_type     = concrete_type
                        }),
                        .source_view = function_type.source_view
                    };
                }),
                .enum_type = concrete_type
            };

            concrete_enum.constructors.push_back(concrete_constructor);
            concrete_enum.associated_namespace->lower_table.add_new_or_abort(concrete_constructor.name.identifier, concrete_constructor);
        }

        auto const concrete_info = utl::wrap(Enum_info {
            .value            = std::move(concrete_enum),
            .home_namespace   = template_info->home_namespace,
            .enumeration_type = concrete_type,
            .state            = Definition_state::resolved,
            .name             = template_info->name,
            .template_instantiation_info = Template_instantiation_info<Enum_template_info> {
                template_info,
                enum_template.parameters,
                std::move(template_arguments)
            }
        });
        *concrete_type.value = mir::type::Enumeration {
            .info           = concrete_info,
            .is_application = true
        };

        enum_template.instantiations.push_back(concrete_info);
        return concrete_info;
    }


    auto instantiate_alias_template_application(
        Context                              & resolution_context,
        mir::Alias_template                  & alias_template,
        utl::Wrapper<Alias_template_info> const template_info,
        std::vector<mir::Template_argument> && template_arguments,
        Scope                                & scope,
        Namespace                            & space) -> utl::Wrapper<Alias_info>
    {
        Substitutions substitutions { alias_template.parameters, template_arguments };

        Substitution_context substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space,
        };

        return utl::wrap(Alias_info {
            .value = mir::Alias {
                .aliased_type = instantiate(alias_template.definition.aliased_type, substitution_context),
                .name         = alias_template.definition.name
            },
            .home_namespace = template_info->home_namespace,
            .state          = Definition_state::resolved,
            .name           = alias_template.definition.name
        });
    }


    struct Expression_instantiation_visitor {
        using R = mir::Expression::Variant;
        Substitution_context context;
        utl::Source_view this_expression_source_view;

        auto operator()(mir::expression::Tuple const& tuple) -> R {
            return mir::expression::Tuple {
                .fields = utl::map(context.recurse(), tuple.fields)
            };
        }
        auto operator()(mir::expression::Loop const& loop) -> R {
            return mir::expression::Loop {
                .body = context.recurse(loop.body)
            };
        }
        auto operator()(mir::expression::Break const& break_) -> R {
            return mir::expression::Break {
                .result = context.recurse(break_.result)
            };
        }
        auto operator()(mir::expression::Continue const&) -> R {
            return mir::expression::Continue {};
        }
        auto operator()(mir::expression::Array_literal const& literal) -> R {
            return mir::expression::Array_literal {
                .elements = utl::map(context.recurse(), literal.elements)
            };
        }
        auto operator()(mir::expression::Block const& block) -> R {
            return mir::expression::Block {
                .side_effect_expressions = utl::map(context.recurse(), block.side_effect_expressions),
                .result_expression       = context.recurse(block.result_expression)
            };
        }
        auto operator()(mir::expression::Direct_invocation const& invocation) -> R {
            return mir::expression::Direct_invocation {
                .function  = invocation.function,
                .arguments = utl::map(context.recurse(), invocation.arguments)
            };
        }
        auto operator()(mir::expression::Indirect_invocation const& invocation) -> R {
            return mir::expression::Indirect_invocation {
                .arguments = utl::map(context.recurse(), invocation.arguments),
                .invocable = context.recurse(invocation.invocable)
            };
        }
        auto operator()(mir::expression::Direct_enum_constructor_invocation const& invocation) -> R {
            return mir::expression::Direct_enum_constructor_invocation {
                .constructor = invocation.constructor,
                .arguments   = utl::map(context.recurse(), invocation.arguments)
            };
        }
        auto operator()(mir::expression::Let_binding const& binding) -> R {
            return mir::expression::Let_binding {
                .pattern     = context.recurse(binding.pattern),
                .type        = context.recurse(binding.type),
                .initializer = context.recurse(binding.initializer)
            };
        }
        auto operator()(mir::expression::Conditional const& conditional) -> R {
            return mir::expression::Conditional {
                .condition    = context.recurse(conditional.condition),
                .true_branch  = context.recurse(conditional.true_branch),
                .false_branch = context.recurse(conditional.false_branch)
            };
        }
        auto operator()(mir::expression::Match const& match) -> R {
            auto const instantiate_case = [this](mir::expression::Match::Case const& match_case) {
                return mir::expression::Match::Case {
                    .pattern = context.recurse(match_case.pattern),
                    .handler = context.recurse(match_case.handler)
                };
            };

            return mir::expression::Match {
                .cases              = utl::map(instantiate_case, match.cases),
                .matched_expression = context.recurse(match.matched_expression)
            };
        }
        auto operator()(mir::expression::Sizeof const& sizeof_) -> R {
            return mir::expression::Sizeof {
                .inspected_type = context.recurse(sizeof_.inspected_type)
            };
        }
        auto operator()(mir::expression::Reference const& reference) -> R {
            return mir::expression::Reference {
                .mutability            = context.recurse(reference.mutability),
                .referenced_expression = context.recurse(reference.referenced_expression)
            };
        }
        auto operator()(mir::expression::Dereference const& dereference) -> R {
            return mir::expression::Dereference {
                .dereferenced_expression = context.recurse(dereference.dereferenced_expression)
            };
        }
        auto operator()(mir::expression::Addressof const& addressof) -> R {
            return mir::expression::Addressof {
                .lvalue = context.recurse(addressof.lvalue)
            };
        }
        auto operator()(mir::expression::Unsafe_dereference const& dereference) -> R {
            return mir::expression::Unsafe_dereference {
                .pointer = context.recurse(dereference.pointer)
            };
        }
        auto operator()(mir::expression::Struct_initializer const& initializer) -> R {
            return mir::expression::Struct_initializer {
                .initializers = utl::map(context.recurse(), initializer.initializers),
                .struct_type  = context.recurse(initializer.struct_type)
            };
        }
        auto operator()(mir::expression::Struct_field_access const& access) -> R {
            return mir::expression::Struct_field_access {
                .base_expression = context.recurse(access.base_expression),
                .field_name      = access.field_name,
            };
        }
        auto operator()(mir::expression::Tuple_field_access const& access) -> R {
            return mir::expression::Tuple_field_access {
                .base_expression         = context.recurse(access.base_expression),
                .field_index             = access.field_index,
                .field_index_source_view = access.field_index_source_view
            };
        }
        auto operator()(mir::expression::Move const& move) -> R {
            return mir::expression::Move { .lvalue = context.recurse(move.lvalue) };
        }

        auto operator()(mir::expression::Function_reference const& function) -> R {
            if (function.is_application) {
                auto const& instantiation_info =
                    utl::get(function.info->template_instantiation_info);

                utl::Wrapper<Function_template_info> const template_info =
                    instantiation_info.template_instantiated_from;

                mir::Function_template& function_template =
                    context.resolution_context.resolve_function_template(template_info);

                return mir::expression::Function_reference {
                    .info = instantiate_function_template_application(
                        context.resolution_context,
                        function_template,
                        template_info,
                        utl::map(context.recurse(), instantiation_info.template_arguments),
                        context.scope,
                        context.space
                    ),
                    .is_application = true
                };
            }
            return function;
        }

        template <class T>
        auto operator()(T const& terminal) -> R
            requires utl::instance_of<T, mir::expression::Literal> ||
                utl::one_of<
                    T,
                    mir::expression::Enum_constructor_reference,
                    mir::expression::Local_variable_reference,
                    mir::expression::Hole
                >
        {
            return terminal;
        }
    };


    struct Type_instantiation_visitor {
        using R = utl::Wrapper<mir::Type::Variant>;
        Substitution_context context;
        mir::Type            this_type;

        auto operator()(mir::type::Template_parameter_reference const& reference) -> R {
            if (mir::Type const* const substitution =
                context.substitutions.type_substitutions.find(reference.tag))
            {
                return context.recurse(*substitution).value;
            }
            return this_type.value;
        }

        auto operator()(mir::type::Tuple const& tuple) -> R {
            return wrap_type(mir::type::Tuple {
                .field_types = utl::map(context.recurse(), tuple.field_types)
            });
        }
        auto operator()(mir::type::Array const& array) -> R {
            return wrap_type(mir::type::Array {
                .element_type = context.recurse(array.element_type),
                .array_length = context.recurse(array.array_length)
            });
        }
        auto operator()(mir::type::Slice const& slice) -> R {
            return wrap_type(mir::type::Slice {
                .element_type = context.recurse(slice.element_type)
            });
        }
        auto operator()(mir::type::Function const& function) -> R {
            return wrap_type(mir::type::Function {
                .parameter_types = utl::map(context.recurse(), function.parameter_types),
                .return_type     = context.recurse(function.return_type)
            });
        }
        auto operator()(mir::type::Reference const& reference) -> R {
            return wrap_type(mir::type::Reference {
                .mutability      = context.recurse(reference.mutability),
                .referenced_type = context.recurse(reference.referenced_type)
            });
        }
        auto operator()(mir::type::Pointer const& pointer) -> R {
            return wrap_type(mir::type::Pointer {
                .mutability      = context.recurse(pointer.mutability),
                .pointed_to_type = context.recurse(pointer.pointed_to_type)
            });
        }
        auto operator()(mir::type::Structure const& structure) -> R {
            if (structure.is_application) {
                auto& instantiation_info =
                    utl::get(structure.info->template_instantiation_info);

                return wrap_type(mir::type::Structure {
                    .info = instantiate_struct_template_application(
                        context.resolution_context,
                        context.resolution_context.resolve_struct_template(instantiation_info.template_instantiated_from),
                        instantiation_info.template_instantiated_from,
                        utl::map(context.recurse(), instantiation_info.template_arguments),
                        context.scope,
                        context.space
                    ),
                    .is_application = true
                });
            }
            return this_type.value;
        }
        auto operator()(mir::type::Enumeration const& enumeration) -> R {
            if (enumeration.is_application) {
                auto& instantiation_info =
                    utl::get(enumeration.info->template_instantiation_info);

                return wrap_type(mir::type::Enumeration {
                    .info = instantiate_enum_template_application(
                        context.resolution_context,
                        context.resolution_context.resolve_enum_template(instantiation_info.template_instantiated_from),
                        instantiation_info.template_instantiated_from,
                        utl::map(context.recurse(), instantiation_info.template_arguments),
                        context.scope,
                        context.space
                    ),
                    .is_application = true
                });
            }
            return this_type.value;
        }

        auto operator()(
            utl::one_of<
                mir::type::Integer,
                mir::type::Floating,
                mir::type::Character,
                mir::type::Boolean,
                mir::type::String,
                mir::type::Self_placeholder,
                mir::type::General_unification_variable,
                mir::type::Integral_unification_variable
            > auto const&) -> R
        {
            return this_type.value;
        }
    };


    struct Pattern_instantiation_visitor {
        Substitution_context context;

        auto operator()(mir::pattern::As const& as) -> mir::Pattern::Variant {
            return mir::pattern::As {
                .alias           = as.alias,
                .aliased_pattern = context.recurse(as.aliased_pattern)
            };
        }
        auto operator()(mir::pattern::Enum_constructor const& pattern) -> mir::Pattern::Variant {
            mir::Type const enum_type =
                context.recurse(pattern.constructor.enum_type);

            mir::Enum& enumeration =
                context.resolution_context.resolve_enum(utl::get<mir::type::Enumeration>(*enum_type.value).info);

            auto it = ranges::find(enumeration.constructors, pattern.constructor.name, &mir::Enum_constructor::name);
            if (it == enumeration.constructors.end()) {
                // Should be unreachable because enum resolution would've caught the error
                utl::unreachable();
            }
            return mir::pattern::Enum_constructor {
                    .payload_pattern = pattern.payload_pattern.transform(context.recurse()),
                    .constructor     = *it
            };
        }
        auto operator()(mir::pattern::Guarded const& guarded) -> mir::Pattern::Variant {
            return mir::pattern::Guarded {
                .guarded_pattern = context.recurse(guarded.guarded_pattern),
                .guard           = context.recurse(guarded.guard)
            };
        }
        auto operator()(mir::pattern::Tuple const& tuple) -> mir::Pattern::Variant {
            return mir::pattern::Tuple { .field_patterns = utl::map(context.recurse(), tuple.field_patterns) };
        }
        auto operator()(mir::pattern::Slice const& slice) -> mir::Pattern::Variant {
            return mir::pattern::Slice { .element_patterns = utl::map(context.recurse(), slice.element_patterns) };
        }
        template <class T>
        auto operator()(T const terminal) -> mir::Pattern::Variant
            requires utl::instance_of<T, mir::pattern::Literal>
                  || utl::one_of<T, mir::pattern::Wildcard, mir::pattern::Name>
        {
            return terminal;
        }
    };



    auto instantiate(mir::Expression const& expression, Substitution_context const context)
        -> mir::Expression
    {
        return {
            .value          = std::visit(Expression_instantiation_visitor { context, expression.source_view }, expression.value),
            .type           = instantiate(expression.type, context),
            .source_view    = expression.source_view,
            .mutability     = instantiate(expression.mutability, context),
            .is_addressable = expression.is_addressable,
        };
    }

    auto instantiate(mir::Type const& type, Substitution_context const context)
        -> mir::Type
    {
        return {
            .value       = std::visit(Type_instantiation_visitor { context, type }, *type.value),
            .source_view = type.source_view
        };
    }

    auto instantiate(mir::Pattern const& pattern, Substitution_context const context)
        -> mir::Pattern
    {
        return {
            .value                   = std::visit(Pattern_instantiation_visitor { context }, pattern.value),
            .type                    = instantiate(pattern.type, context),
            .is_exhaustive_by_itself = pattern.is_exhaustive_by_itself,
            .source_view             = pattern.source_view,
        };
    }

    auto instantiate(mir::Mutability const& mutability, Substitution_context const context)
        -> mir::Mutability
    {
        if (auto const* parameterized = std::get_if<mir::Mutability::Parameterized>(&*mutability.value)) {
            if (mir::Mutability const* const substitution =
                context.substitutions.mutability_substitutions.find(parameterized->tag))
            {
                return context.recurse(*substitution);
            }
        }
        return mutability;
    }

    auto instantiate(mir::Template_argument const& argument, Substitution_context const context)
        -> mir::Template_argument
    {
        return mir::Template_argument {
            .value = std::visit<mir::Template_argument::Variant>(context.recurse(), argument.value),
            .name  = argument.name
        };
    }

    auto instantiate(mir::Self_parameter const& parameter, Substitution_context const context)
        -> mir::Self_parameter
    {
        return mir::Self_parameter {
            .mutability   = instantiate(parameter.mutability, context),
            .is_reference = parameter.is_reference,
            .source_view  = parameter.source_view
        };
    }


    [[nodiscard]]
    auto synthetize_arguments(utl::Usize const parameter_count, utl::Source_view const argument_view) -> std::vector<hir::Template_argument> {
        return std::vector(parameter_count, hir::Template_argument { hir::Template_argument::Wildcard { argument_view } });
    }

}


auto resolution::Context::instantiate_function_template(
    utl::Wrapper<Function_template_info>     const template_info,
    std::span<hir::Template_argument const> const template_arguments,
    utl::Source_view                         const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Function_info>
{
    mir::Function_template& function_template =
        resolve_function_template(template_info);

    return instantiate_function_template_application(
        *this,
        function_template,
        template_info,
        resolve_template_arguments(*this, function_template.parameters, template_arguments, instantiation_view, scope, space),
        scope,
        space
    );
}


auto resolution::Context::instantiate_struct_template(
    utl::Wrapper<Struct_template_info>       const template_info,
    std::span<hir::Template_argument const> const template_arguments,
    utl::Source_view                         const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Struct_info>
{
    mir::Struct_template& struct_template =
        resolve_struct_template(template_info);

    return instantiate_struct_template_application(
        *this,
        struct_template,
        template_info,
        resolve_template_arguments(*this, struct_template.parameters, template_arguments, instantiation_view, scope, space),
        scope,
        space
    );
}


auto resolution::Context::instantiate_enum_template(
    utl::Wrapper<Enum_template_info>        const template_info,
    std::span<hir::Template_argument const> const template_arguments,
    utl::Source_view                        const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Enum_info>
{
    mir::Enum_template& enum_template =
        resolve_enum_template(template_info);

    return instantiate_enum_template_application(
        *this,
        enum_template,
        template_info,
        resolve_template_arguments(*this, enum_template.parameters, template_arguments, instantiation_view, scope, space),
        scope,
        space
    );
}


auto resolution::Context::instantiate_alias_template(
    utl::Wrapper<Alias_template_info>       const template_info,
    std::span<hir::Template_argument const> const template_arguments,
    utl::Source_view                        const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Alias_info>
{
    mir::Alias_template& alias_template =
        resolve_alias_template(template_info);

    return instantiate_alias_template_application(
        *this,
        alias_template,
        template_info,
        resolve_template_arguments(*this, alias_template.parameters, template_arguments, instantiation_view, scope, space),
        scope,
        space
    );
}


auto resolution::Context::instantiate_function_template_with_synthetic_arguments(
    utl::Wrapper<Function_template_info> const template_info,
    utl::Source_view                     const instantiation_view) -> utl::Wrapper<Function_info>
{
    auto const arguments = synthetize_arguments(resolve_function_template(template_info).parameters.size(), instantiation_view);
    Scope instantiation_scope { *this };
    return instantiate_function_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}

auto resolution::Context::instantiate_struct_template_with_synthetic_arguments(
    utl::Wrapper<Struct_template_info> const template_info,
    utl::Source_view                   const instantiation_view) -> utl::Wrapper<Struct_info>
{
    auto const arguments = synthetize_arguments(resolve_struct_template(template_info).parameters.size(), instantiation_view);
    Scope instantiation_scope { *this };
    return instantiate_struct_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}

auto resolution::Context::instantiate_enum_template_with_synthetic_arguments(
    utl::Wrapper<Enum_template_info> const template_info,
    utl::Source_view                 const instantiation_view) -> utl::Wrapper<Enum_info>
{
    auto const arguments = synthetize_arguments(resolve_enum_template(template_info).parameters.size(), instantiation_view);
    Scope instantiation_scope { *this };
    return instantiate_enum_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}

auto resolution::Context::instantiate_alias_template_with_synthetic_arguments(
    utl::Wrapper<Alias_template_info> const template_info,
    utl::Source_view                  const instantiation_view) -> utl::Wrapper<Alias_info>
{
    auto const arguments = synthetize_arguments(resolve_alias_template(template_info).parameters.size(), instantiation_view);
    Scope instantiation_scope { *this };
    return instantiate_alias_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}
