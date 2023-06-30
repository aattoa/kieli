#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;


namespace {

    struct [[nodiscard]] Substitutions {
        utl::Flatmap<hir::Template_parameter_tag, hir::Type>       type_substitutions;
        utl::Flatmap<hir::Template_parameter_tag, hir::Mutability> mutability_substitutions;

        auto add_substitution(hir::Template_parameter const& parameter, hir::Template_argument const& argument) -> void {
            std::visit(utl::Overload {
                [&](hir::Template_parameter::Type_parameter const&, hir::Type const type_argument) {
                    type_substitutions.add_new_or_abort(parameter.reference_tag, type_argument);
                },
                [&](hir::Template_parameter::Mutability_parameter, hir::Mutability const mutability_argument) {
                    mutability_substitutions.add_new_or_abort(parameter.reference_tag, mutability_argument);
                },
                [&](auto const&, auto const&) {
                    utl::unreachable();
                }
            }, parameter.value, argument.value);
        }

        Substitutions(
            std::span<hir::Template_parameter const> const parameters,
            std::span<hir::Template_argument  const> const arguments)
        {
            for (auto [parameter, argument] : ranges::views::zip(parameters, arguments))
                add_substitution(parameter, argument);
        }
    };


    struct [[nodiscard]] Substitution_context;

    auto instantiate(hir::Expression        const&, Substitution_context) -> hir::Expression;
    auto instantiate(hir::Type              const&, Substitution_context) -> hir::Type;
    auto instantiate(hir::Pattern           const&, Substitution_context) -> hir::Pattern;
    auto instantiate(hir::Mutability        const&, Substitution_context) -> hir::Mutability;
    auto instantiate(hir::Template_argument const&, Substitution_context) -> hir::Template_argument;
    auto instantiate(hir::Self_parameter    const&, Substitution_context) -> hir::Self_parameter;

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
            return resolution_context.wrap(recurse(*wrapper));
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


    auto validate_template_argument_count(
        Context              & context,
        std::string_view const template_name,
        utl::Source_view const instantiation_view,
        utl::Usize       const minimum_argument_count,
        utl::Usize       const maximum_argument_count,
        utl::Usize       const actual_argument_count) -> void
    {
        if (minimum_argument_count == maximum_argument_count) {
            // There are no parameters with default arguments
            if (actual_argument_count != minimum_argument_count) {
                if (maximum_argument_count == 0) {
                    context.error(instantiation_view, {
                        .message = std::format(
                            "{} has no explicit template parameters, but {} explicit template {} supplied",
                            template_name,
                            actual_argument_count,
                            actual_argument_count == 1 ? "argument was" : "arguments were")
                    });
                }
                else {
                    context.error(instantiation_view, {
                        .message = std::format(
                            "{} requires exactly {} template {}, but {} {} supplied",
                            template_name,
                            minimum_argument_count,
                            minimum_argument_count == 1 ? "argument" : "arguments",
                            actual_argument_count,
                            actual_argument_count == 1 ? "was" : "were")
                    });
                }
            }
        }
        else {
            // There are parameters with default arguments
            if (actual_argument_count < minimum_argument_count) {
                // Too few arguments
                context.error(instantiation_view, {
                    .message = std::format(
                        "{} requires at least {} template {}, but {} {} supplied",
                        template_name,
                        minimum_argument_count,
                        minimum_argument_count == 1 ? "argument" : "arguments",
                        actual_argument_count,
                        actual_argument_count == 1 ? "was" : "were")
                });
            }
            else if (actual_argument_count > maximum_argument_count) {
                // Too many arguments
                context.error(instantiation_view, {
                    .message = std::format(
                        "{} has only {} template {}, but {} template {} supplied",
                        template_name,
                        maximum_argument_count,
                        maximum_argument_count == 1 ? "parameter" : "parameters",
                        actual_argument_count,
                        actual_argument_count == 1 ? "argument was" : "arguments were")
                });
            }
        }
    }

    auto resolve_single_template_argument(
        Context                      & context,
        Scope                        & scope,
        Namespace                    & space,
        hir::Template_parameter const& parameter,
        ast::Template_argument  const& argument,
        utl::Source_view        const  instantiation_view) -> hir::Template_argument
    {
        return std::visit(utl::Overload {
            [&](hir::Template_parameter::Type_parameter const& type_parameter, utl::Wrapper<ast::Type> const type_argument) {
                if (!type_parameter.classes.empty()) { utl::todo(); }
                return hir::Template_argument { context.resolve_type(*type_argument, scope, space) };
            },
            [&](hir::Template_parameter::Type_parameter const& type_parameter, ast::Template_argument::Wildcard const wildcard) {
                utl::wrapper auto const state = context.fresh_unification_type_variable_state(hir::Unification_type_variable_kind::general);
                state->as_unsolved().classes = type_parameter.classes;
                return hir::Template_argument { hir::Type { context.wrap_type(hir::type::Unification_variable { .state = state }), wildcard.source_view } };
            },
            [&](hir::Template_parameter::Mutability_parameter, ast::Mutability const mutability_argument) {
                return hir::Template_argument { context.resolve_mutability(mutability_argument, scope) };
            },
            [&](hir::Template_parameter::Mutability_parameter, ast::Template_argument::Wildcard const wildcard) {
                return hir::Template_argument { context.fresh_unification_mutability_variable(wildcard.source_view) };
            },
            [&](auto const&, auto const&) -> hir::Template_argument {
                context.error(instantiation_view, {
                    .message = std::format(
                        "Argument {} is incompatible with parameter {}",
                        ast::to_string(argument),
                        hir::to_string(parameter))
                });
            },
        }, parameter.value, argument.value);
    }

    auto resolve_explicit_template_arguments(
        Context                                      & context,
        Scope                                        & scope,
        Namespace                                    & space,
        std::vector<hir::Template_argument>          & output_arguments,
        std::span<hir::Template_parameter const> const parameters,
        std::span<ast::Template_argument  const> const arguments,
        utl::Source_view                         const instantiation_view) -> void
    {
        utl::always_assert(parameters.size() == arguments.size());
        for (auto const& [parameter, argument] : ranges::views::zip(parameters, arguments))
            output_arguments.push_back(resolve_single_template_argument(context, scope, space, parameter, argument, instantiation_view));
    }

    auto resolve_defaulted_template_arguments(
        Context                                      & context,
        Namespace                                    & template_space,
        std::vector<hir::Template_argument>          & output_arguments,
        Substitutions                                & substitutions,
        Substitution_context                     const substitution_context,
        std::span<hir::Template_parameter const> const parameters,
        utl::Source_view                         const instantiation_view) -> void
    {
        Scope empty_scope;
        for (hir::Template_parameter const& parameter : parameters) {
            utl::always_assert(parameter.default_argument.has_value());
            hir::Template_argument default_argument = substitution_context.recurse(
                resolve_single_template_argument(
                    context,
                    parameter.default_argument->scope
                        ? *parameter.default_argument->scope
                        : empty_scope,
                    template_space,
                    parameter,
                    parameter.default_argument->argument,
                    instantiation_view));
            substitutions.add_substitution(parameter, default_argument);
            output_arguments.push_back(std::move(default_argument));
        }
    }

    auto resolve_template_arguments(
        Context                                      & context,
        Scope                                        & scope,
        Namespace                                    & instantiation_space,
        Namespace                                    & template_space,
        std::span<hir::Template_parameter const> const parameters,
        std::span<ast::Template_argument  const> const arguments,
        std::string_view                         const template_name,
        utl::Source_view                         const instantiation_view) -> std::vector<hir::Template_argument>
    {
        auto const first_defaulted = ranges::find_if(parameters, [](auto const& parameter) { return parameter.default_argument.has_value(); });
        auto const first_implicit  = ranges::find_if(parameters, &hir::Template_parameter::is_implicit);

        utl::Usize const minimum_required_argument_count  = utl::unsigned_distance(parameters.begin(), first_defaulted);
        utl::Usize const maximum_permitted_argument_count = utl::unsigned_distance(parameters.begin(), first_implicit);

        validate_template_argument_count(
            context,
            template_name,
            instantiation_view,
            minimum_required_argument_count,
            maximum_permitted_argument_count,
            arguments.size());

        utl::Usize const defaulted_argument_count = utl::unsigned_distance(first_defaulted, parameters.end());
        utl::always_assert(parameters.size() == minimum_required_argument_count + defaulted_argument_count);

        std::vector<hir::Template_argument> hir_arguments;
        hir_arguments.reserve(parameters.size());

        resolve_explicit_template_arguments(
            context,
            scope,
            instantiation_space,
            hir_arguments,
            parameters.subspan(0, arguments.size()),
            arguments,
            instantiation_view);

        Substitutions default_argument_substitutions { parameters.subspan(0, arguments.size()), hir_arguments };
        Substitution_context const default_argument_substitution_context { default_argument_substitutions, context, scope, instantiation_space };

        resolve_defaulted_template_arguments(
            context,
            template_space,
            hir_arguments,
            default_argument_substitutions,
            default_argument_substitution_context,
            parameters.subspan(arguments.size()),
            instantiation_view);

        return hir_arguments;
    }


    auto instantiate_function_template_application(
        Context                             & resolution_context,
        hir::Function                       & function_template,
        utl::Wrapper<Function_info>     const template_info,
        std::vector<hir::Template_argument>&& template_arguments,
        Scope                               & scope,
        Namespace                           & space) -> utl::Wrapper<Function_info>
    {
        utl::always_assert(function_template.signature.is_template());
        Substitutions substitutions { function_template.signature.template_parameters, template_arguments };

        Substitution_context const substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space,
        };

        auto const instantiate_parameter = [=](hir::Function_parameter const& parameter) {
            return hir::Function_parameter {
                .pattern = instantiate(parameter.pattern, substitution_context),
                .type    = instantiate(parameter.type,    substitution_context),
            };
        };

        tl::optional<hir::Self_parameter> concrete_self_parameter =
            function_template.signature.self_parameter.transform(substitution_context.recurse());

        std::vector<hir::Function_parameter> concrete_function_parameters =
            utl::map(instantiate_parameter, function_template.signature.parameters);

        hir::Type const concrete_return_type =
            instantiate(function_template.signature.return_type, substitution_context);

        hir::Type const concrete_function_type {
            resolution_context.wrap_type(hir::type::Function {
                .parameter_types = utl::map(&hir::Function_parameter::type, concrete_function_parameters),
                .return_type     = concrete_return_type,
            }),
            template_info->name.source_view,
        };

        hir::Function concrete_function {
            .signature {
                .parameters     = std::move(concrete_function_parameters),
                .self_parameter = std::move(concrete_self_parameter),
                .name           = function_template.signature.name,
                .return_type    = concrete_return_type,
                .function_type  = concrete_function_type,
            },
            .body = instantiate(function_template.body, substitution_context),
        };

        auto const info = resolution_context.wrap(Function_info {
            .value          = std::move(concrete_function),
            .home_namespace = template_info->home_namespace,
            .state          = Definition_state::resolved,
            .name           = template_info->name,
            .template_instantiation_info = Template_instantiation_info<Function_info> {
                template_info,
                function_template.signature.template_parameters,
                std::move(template_arguments),
            }
        });
        function_template.template_instantiations.push_back(info);
        return info;
    }


    auto instantiate_struct_template_application(
        Context                                & resolution_context,
        hir::Struct_template                   & struct_template,
        utl::Wrapper<Struct_template_info> const template_info,
        std::vector<hir::Template_argument>   && template_arguments,
        Scope                                  & scope,
        Namespace                              & space) -> utl::Wrapper<Struct_info>
    {
        Substitutions substitutions { struct_template.parameters, template_arguments };

        Substitution_context const substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space,
        };

        hir::Struct concrete_struct {
            .members              = utl::vector_with_capacity(struct_template.definition.members.size()),
            .name                 = template_info->name,
            .associated_namespace = resolution_context.wrap(Namespace {}),
        };

        for (hir::Struct::Member const& member : struct_template.definition.members) {
            concrete_struct.members.push_back(
                hir::Struct::Member {
                    .name      = member.name,
                    .type      = instantiate(member.type, substitution_context),
                    .is_public = member.is_public,
                }
            );
        }

        hir::Type const concrete_type =
            resolution_context.temporary_placeholder_type(concrete_struct.name.source_view);

        auto const concrete_info = resolution_context.wrap(Struct_info {
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
        *concrete_type.pure_value() = hir::type::Structure {
            .info           = concrete_info,
            .is_application = true,
        };

        struct_template.instantiations.push_back(concrete_info);
        return concrete_info;
    }


    auto instantiate_enum_template_application(
        Context                              & resolution_context,
        hir::Enum_template                   & enum_template,
        utl::Wrapper<Enum_template_info> const template_info,
        std::vector<hir::Template_argument> && template_arguments,
        Scope                                & scope,
        Namespace                            & space) -> utl::Wrapper<Enum_info>
    {
        Substitutions substitutions { enum_template.parameters, template_arguments };

        Substitution_context const substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space,
        };

        hir::Enum concrete_enum {
            .constructors         = utl::vector_with_capacity(enum_template.definition.constructors.size()),
            .name                 = template_info->name,
            .associated_namespace = resolution_context.wrap(Namespace { .parent = template_info->home_namespace })
        };

        hir::Type concrete_type =
            resolution_context.temporary_placeholder_type(concrete_enum.name.source_view);


        for (hir::Enum_constructor& constructor : enum_template.definition.constructors) {
            hir::Enum_constructor concrete_constructor {
                .name          = constructor.name,
                .payload_type  = constructor.payload_type.transform(substitution_context.recurse()),
                .function_type = constructor.function_type.transform([&](hir::Type const function_type) {
                    return hir::Type {
                        resolution_context.wrap_type(hir::type::Function {
                            .parameter_types = utl::map(substitution_context.recurse(), utl::get<hir::type::Function>(*function_type.flattened_value()).parameter_types),
                            .return_type     = concrete_type,
                        }),
                        function_type.source_view(),
                    };
                }),
                .enum_type = concrete_type,
            };

            concrete_enum.constructors.push_back(concrete_constructor);
            concrete_enum.associated_namespace->lower_table.add_new_or_abort(concrete_constructor.name.identifier, concrete_constructor);
        }

        auto const concrete_info = resolution_context.wrap(Enum_info {
            .value            = std::move(concrete_enum),
            .home_namespace   = template_info->home_namespace,
            .enumeration_type = concrete_type,
            .state            = Definition_state::resolved,
            .name             = template_info->name,
            .template_instantiation_info = Template_instantiation_info<Enum_template_info> {
                template_info,
                enum_template.parameters,
                std::move(template_arguments),
            }
        });
        *concrete_type.pure_value() = hir::type::Enumeration {
            .info           = concrete_info,
            .is_application = true,
        };

        enum_template.instantiations.push_back(concrete_info);
        return concrete_info;
    }


    auto instantiate_alias_template_application(
        Context                               & resolution_context,
        hir::Alias_template                   & alias_template,
        utl::Wrapper<Alias_template_info> const template_info,
        std::vector<hir::Template_argument>  && template_arguments,
        Scope                                 & scope,
        Namespace                             & space) -> utl::Wrapper<Alias_info>
    {
        Substitutions substitutions { alias_template.parameters, template_arguments };

        Substitution_context const substitution_context {
            .substitutions      = substitutions,
            .resolution_context = resolution_context,
            .scope              = scope,
            .space              = space,
        };

        return resolution_context.wrap(Alias_info {
            .value = hir::Alias {
                .name         = alias_template.definition.name,
                .aliased_type = instantiate(alias_template.definition.aliased_type, substitution_context),
            },
            .home_namespace = template_info->home_namespace,
            .state          = Definition_state::resolved,
            .name           = alias_template.definition.name,
        });
    }


    struct Expression_instantiation_visitor {
        using R = hir::Expression::Variant;
        Substitution_context context;

        auto operator()(hir::expression::Tuple const& tuple) -> R {
            return hir::expression::Tuple {
                .fields = utl::map(context.recurse(), tuple.fields),
            };
        }
        auto operator()(hir::expression::Loop const& loop) -> R {
            return hir::expression::Loop {
                .body = context.recurse(loop.body),
            };
        }
        auto operator()(hir::expression::Break const& break_) -> R {
            return hir::expression::Break {
                .result = context.recurse(break_.result),
            };
        }
        auto operator()(hir::expression::Continue const&) -> R {
            return hir::expression::Continue {};
        }
        auto operator()(hir::expression::Array_literal const& literal) -> R {
            return hir::expression::Array_literal {
                .elements = utl::map(context.recurse(), literal.elements),
            };
        }
        auto operator()(hir::expression::Block const& block) -> R {
            return hir::expression::Block {
                .side_effect_expressions = utl::map(context.recurse(), block.side_effect_expressions),
                .result_expression       = context.recurse(block.result_expression),
            };
        }
        auto operator()(hir::expression::Direct_invocation const& invocation) -> R {
            return hir::expression::Direct_invocation {
                .function  = invocation.function,
                .arguments = utl::map(context.recurse(), invocation.arguments),
            };
        }
        auto operator()(hir::expression::Indirect_invocation const& invocation) -> R {
            return hir::expression::Indirect_invocation {
                .arguments = utl::map(context.recurse(), invocation.arguments),
                .invocable = context.recurse(invocation.invocable),
            };
        }
        auto operator()(hir::expression::Direct_enum_constructor_invocation const& invocation) -> R {
            return hir::expression::Direct_enum_constructor_invocation {
                .constructor = invocation.constructor,
                .arguments   = utl::map(context.recurse(), invocation.arguments),
            };
        }
        auto operator()(hir::expression::Let_binding const& binding) -> R {
            return hir::expression::Let_binding {
                .pattern     = context.recurse(binding.pattern),
                .type        = context.recurse(binding.type),
                .initializer = context.recurse(binding.initializer),
            };
        }
        auto operator()(hir::expression::Conditional const& conditional) -> R {
            return hir::expression::Conditional {
                .condition    = context.recurse(conditional.condition),
                .true_branch  = context.recurse(conditional.true_branch),
                .false_branch = context.recurse(conditional.false_branch),
            };
        }
        auto operator()(hir::expression::Match const& match) -> R {
            auto const instantiate_case = [this](hir::expression::Match::Case const& match_case) {
                return hir::expression::Match::Case {
                    .pattern = context.recurse(match_case.pattern),
                    .handler = context.recurse(match_case.handler),
                };
            };

            return hir::expression::Match {
                .cases              = utl::map(instantiate_case, match.cases),
                .matched_expression = context.recurse(match.matched_expression),
            };
        }
        auto operator()(hir::expression::Sizeof const& sizeof_) -> R {
            return hir::expression::Sizeof {
                .inspected_type = context.recurse(sizeof_.inspected_type),
            };
        }
        auto operator()(hir::expression::Reference const& reference) -> R {
            return hir::expression::Reference {
                .mutability            = context.recurse(reference.mutability),
                .referenced_expression = context.recurse(reference.referenced_expression),
            };
        }
        auto operator()(hir::expression::Dereference const& dereference) -> R {
            return hir::expression::Dereference {
                .dereferenced_expression = context.recurse(dereference.dereferenced_expression),
            };
        }
        auto operator()(hir::expression::Addressof const& addressof) -> R {
            return hir::expression::Addressof {
                .lvalue = context.recurse(addressof.lvalue),
            };
        }
        auto operator()(hir::expression::Unsafe_dereference const& dereference) -> R {
            return hir::expression::Unsafe_dereference {
                .pointer = context.recurse(dereference.pointer),
            };
        }
        auto operator()(hir::expression::Struct_initializer const& initializer) -> R {
            return hir::expression::Struct_initializer {
                .initializers = utl::map(context.recurse(), initializer.initializers),
                .struct_type  = context.recurse(initializer.struct_type),
            };
        }
        auto operator()(hir::expression::Struct_field_access const& access) -> R {
            return hir::expression::Struct_field_access {
                .base_expression = context.recurse(access.base_expression),
                .field_name      = access.field_name,
            };
        }
        auto operator()(hir::expression::Tuple_field_access const& access) -> R {
            return hir::expression::Tuple_field_access {
                .base_expression         = context.recurse(access.base_expression),
                .field_index             = access.field_index,
                .field_index_source_view = access.field_index_source_view,
            };
        }
        auto operator()(hir::expression::Move const& move) -> R {
            return hir::expression::Move { .lvalue = context.recurse(move.lvalue) };
        }

        auto operator()(hir::expression::Function_reference const& function) -> R {
            if (function.is_application) {
                auto const& instantiation_info =
                    utl::get(function.info->template_instantiation_info);
                utl::Wrapper<Function_info> const template_info =
                    instantiation_info.template_instantiated_from;
                hir::Function& function_template =
                    context.resolution_context.resolve_function(template_info);

                return hir::expression::Function_reference {
                    .info = instantiate_function_template_application(
                        context.resolution_context,
                        function_template,
                        template_info,
                        utl::map(context.recurse(), instantiation_info.template_arguments),
                        context.scope,
                        context.space),
                    .is_application = true,
                };
            }
            return function;
        }

        template <class T>
            requires compiler::literal<T> || utl::one_of<T, hir::expression::Enum_constructor_reference, hir::expression::Local_variable_reference, hir::expression::Hole>
        auto operator()(T const& terminal) -> R {
            return terminal;
        }
    };


    struct Type_instantiation_visitor {
        using R = utl::Wrapper<hir::Type::Variant>;
        Substitution_context context;
        hir::Type            this_type;

        auto operator()(hir::type::Template_parameter_reference const& reference) -> R {
            if (hir::Type const* const substitution =
                context.substitutions.type_substitutions.find(reference.tag))
            {
                return context.recurse(*substitution).flattened_value();
            }
            return this_type.pure_value();
        }

        auto operator()(hir::type::Tuple const& tuple) -> R {
            return context.resolution_context.wrap_type(hir::type::Tuple {
                .field_types = utl::map(context.recurse(), tuple.field_types),
            });
        }
        auto operator()(hir::type::Array const& array) -> R {
            return context.resolution_context.wrap_type(hir::type::Array {
                .element_type = context.recurse(array.element_type),
                .array_length = context.recurse(array.array_length),
            });
        }
        auto operator()(hir::type::Slice const& slice) -> R {
            return context.resolution_context.wrap_type(hir::type::Slice {
                .element_type = context.recurse(slice.element_type),
            });
        }
        auto operator()(hir::type::Function const& function) -> R {
            return context.resolution_context.wrap_type(hir::type::Function {
                .parameter_types = utl::map(context.recurse(), function.parameter_types),
                .return_type     = context.recurse(function.return_type),
            });
        }
        auto operator()(hir::type::Reference const& reference) -> R {
            return context.resolution_context.wrap_type(hir::type::Reference {
                .mutability      = context.recurse(reference.mutability),
                .referenced_type = context.recurse(reference.referenced_type),
            });
        }
        auto operator()(hir::type::Pointer const& pointer) -> R {
            return context.resolution_context.wrap_type(hir::type::Pointer {
                .mutability      = context.recurse(pointer.mutability),
                .pointed_to_type = context.recurse(pointer.pointed_to_type),
            });
        }
        auto operator()(hir::type::Structure const& structure) -> R {
            if (structure.is_application) {
                auto& instantiation_info =
                    utl::get(structure.info->template_instantiation_info);

                return context.resolution_context.wrap_type(hir::type::Structure {
                    .info = instantiate_struct_template_application(
                        context.resolution_context,
                        context.resolution_context.resolve_struct_template(instantiation_info.template_instantiated_from),
                        instantiation_info.template_instantiated_from,
                        utl::map(context.recurse(), instantiation_info.template_arguments),
                        context.scope,
                        context.space),
                    .is_application = true,
                });
            }
            return this_type.pure_value();
        }
        auto operator()(hir::type::Enumeration const& enumeration) -> R {
            if (enumeration.is_application) {
                auto& instantiation_info =
                    utl::get(enumeration.info->template_instantiation_info);

                return context.resolution_context.wrap_type(hir::type::Enumeration {
                    .info = instantiate_enum_template_application(
                        context.resolution_context,
                        context.resolution_context.resolve_enum_template(instantiation_info.template_instantiated_from),
                        instantiation_info.template_instantiated_from,
                        utl::map(context.recurse(), instantiation_info.template_arguments),
                        context.scope,
                        context.space),
                    .is_application = true,
                });
            }
            return this_type.pure_value();
        }

        auto operator()(
            utl::one_of<
                compiler::built_in_type::Integer,
                compiler::built_in_type::Floating,
                compiler::built_in_type::Character,
                compiler::built_in_type::Boolean,
                compiler::built_in_type::String,
                hir::type::Self_placeholder,
                hir::type::Unification_variable
            > auto const&) -> R
        {
            return this_type.pure_value();
        }
    };


    struct Pattern_instantiation_visitor {
        Substitution_context context;

        auto operator()(hir::pattern::As const& as) -> hir::Pattern::Variant {
            return hir::pattern::As {
                .alias           = as.alias,
                .aliased_pattern = context.recurse(as.aliased_pattern),
            };
        }
        auto operator()(hir::pattern::Enum_constructor const& pattern) -> hir::Pattern::Variant {
            hir::Type const enum_type =
                context.recurse(pattern.constructor.enum_type);

            hir::Enum& enumeration =
                context.resolution_context.resolve_enum(utl::get<hir::type::Enumeration>(*enum_type.pure_value()).info);

            auto it = ranges::find(enumeration.constructors, pattern.constructor.name, &hir::Enum_constructor::name);
            if (it == enumeration.constructors.end()) {
                // Should be unreachable because enum resolution would've caught the error
                utl::unreachable();
            }
            return hir::pattern::Enum_constructor {
                .payload_pattern = pattern.payload_pattern.transform(context.recurse()),
                .constructor     = *it,
            };
        }
        auto operator()(hir::pattern::Guarded const& guarded) -> hir::Pattern::Variant {
            return hir::pattern::Guarded {
                .guarded_pattern = context.recurse(guarded.guarded_pattern),
                .guard           = context.recurse(guarded.guard),
            };
        }
        auto operator()(hir::pattern::Tuple const& tuple) -> hir::Pattern::Variant {
            return hir::pattern::Tuple { .field_patterns = utl::map(context.recurse(), tuple.field_patterns) };
        }
        auto operator()(hir::pattern::Slice const& slice) -> hir::Pattern::Variant {
            return hir::pattern::Slice { .element_patterns = utl::map(context.recurse(), slice.element_patterns) };
        }
        template <class T>
        auto operator()(T const& terminal) -> hir::Pattern::Variant
            requires compiler::literal<T> || utl::one_of<T, hir::pattern::Wildcard, hir::pattern::Name>
        {
            return terminal;
        }
    };



    auto instantiate(hir::Expression const& expression, Substitution_context const context)
        -> hir::Expression
    {
        return {
            .value          = std::visit(Expression_instantiation_visitor { context }, expression.value),
            .type           = instantiate(expression.type, context),
            .source_view    = expression.source_view,
            .mutability     = instantiate(expression.mutability, context),
            .is_addressable = expression.is_addressable,
        };
    }

    auto instantiate(hir::Type const& type, Substitution_context const context) -> hir::Type {
        return hir::Type {
            std::visit(Type_instantiation_visitor { context, type }, *type.flattened_value()),
            type.source_view(),
        };
    }

    auto instantiate(hir::Pattern const& pattern, Substitution_context const context) -> hir::Pattern {
        return {
            .value                   = std::visit(Pattern_instantiation_visitor { context }, pattern.value),
            .is_exhaustive_by_itself = pattern.is_exhaustive_by_itself,
            .source_view             = pattern.source_view,
        };
    }

    auto instantiate(hir::Mutability const& mutability, Substitution_context const context) -> hir::Mutability {
        if (auto const* parameterized = std::get_if<hir::Mutability::Parameterized>(&*mutability.flattened_value())) {
            if (hir::Mutability const* const substitution =
                context.substitutions.mutability_substitutions.find(parameterized->tag))
            {
                return context.recurse(*substitution);
            }
        }
        return mutability;
    }

    auto instantiate(hir::Template_argument const& argument, Substitution_context const context) -> hir::Template_argument {
        return hir::Template_argument {
            .value = std::visit<hir::Template_argument::Variant>(context.recurse(), argument.value),
        };
    }

    auto instantiate(hir::Self_parameter const& parameter, Substitution_context const context) -> hir::Self_parameter {
        return hir::Self_parameter {
            .mutability   = instantiate(parameter.mutability, context),
            .is_reference = parameter.is_reference,
            .source_view  = parameter.source_view
        };
    }


    [[nodiscard]]
    auto synthetize_arguments_for(std::span<hir::Template_parameter const> const parameters, utl::Source_view const argument_view)
        -> std::vector<ast::Template_argument>
    {
        return std::vector(
            ranges::count_if(parameters, std::not_fn(&hir::Template_parameter::is_implicit)),
            ast::Template_argument { ast::Template_argument::Wildcard { argument_view } });
    }

}


auto libresolve::Context::instantiate_function_template(
    utl::Wrapper<Function_info>             const template_info,
    std::span<ast::Template_argument const> const template_arguments,
    utl::Source_view                        const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Function_info>
{
    hir::Function& function = resolve_function(template_info);
    if (!function.signature.is_template()) {
        error(instantiation_view, {
            .message = std::format(
                "{} is not a template, so template arguments can not be applied to it",
                function.signature.name)
        });
    }
    return instantiate_function_template_application(
        *this,
        function,
        template_info,
        resolve_template_arguments(
            *this,
            scope,
            space,
            *template_info->home_namespace,
            function.signature.template_parameters,
            template_arguments,
            function.signature.name.identifier.view(),
            instantiation_view),
        scope,
        space);
}


auto libresolve::Context::instantiate_struct_template(
    utl::Wrapper<Struct_template_info>      const template_info,
    std::span<ast::Template_argument const> const template_arguments,
    utl::Source_view                        const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Struct_info>
{
    hir::Struct_template& struct_template = resolve_struct_template(template_info);
    return instantiate_struct_template_application(
        *this,
        struct_template,
        template_info,
        resolve_template_arguments(
            *this,
            scope,
            space,
            *template_info->home_namespace,
            struct_template.parameters,
            template_arguments,
            struct_template.definition.name.identifier.view(),
            instantiation_view),
        scope,
        space);
}


auto libresolve::Context::instantiate_enum_template(
    utl::Wrapper<Enum_template_info>        const template_info,
    std::span<ast::Template_argument const> const template_arguments,
    utl::Source_view                        const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Enum_info>
{
    hir::Enum_template& enum_template = resolve_enum_template(template_info);
    return instantiate_enum_template_application(
        *this,
        enum_template,
        template_info,
        resolve_template_arguments(
            *this,
            scope,
            space,
            *template_info->home_namespace,
            enum_template.parameters,
            template_arguments,
            enum_template.definition.name.identifier.view(),
            instantiation_view),
        scope,
        space);
}


auto libresolve::Context::instantiate_alias_template(
    utl::Wrapper<Alias_template_info>       const template_info,
    std::span<ast::Template_argument const> const template_arguments,
    utl::Source_view                        const instantiation_view,
    Scope                                       & scope,
    Namespace                                   & space) -> utl::Wrapper<Alias_info>
{
    hir::Alias_template& alias_template = resolve_alias_template(template_info);
    return instantiate_alias_template_application(
        *this,
        alias_template,
        template_info,
        resolve_template_arguments(
            *this,
            scope,
            space,
            *template_info->home_namespace,
            alias_template.parameters,
            template_arguments,
            alias_template.definition.name.identifier.view(),
            instantiation_view),
        scope,
        space);
}


auto libresolve::Context::instantiate_function_template_with_synthetic_arguments(
    utl::Wrapper<Function_info> const template_info,
    utl::Source_view            const instantiation_view) -> utl::Wrapper<Function_info>
{
    auto const arguments = synthetize_arguments_for(resolve_function(template_info).signature.template_parameters, instantiation_view);
    Scope instantiation_scope;
    return instantiate_function_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}

auto libresolve::Context::instantiate_struct_template_with_synthetic_arguments(
    utl::Wrapper<Struct_template_info> const template_info,
    utl::Source_view                   const instantiation_view) -> utl::Wrapper<Struct_info>
{
    auto const arguments = synthetize_arguments_for(resolve_struct_template(template_info).parameters, instantiation_view);
    Scope instantiation_scope;
    return instantiate_struct_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}

auto libresolve::Context::instantiate_enum_template_with_synthetic_arguments(
    utl::Wrapper<Enum_template_info> const template_info,
    utl::Source_view                 const instantiation_view) -> utl::Wrapper<Enum_info>
{
    auto const arguments = synthetize_arguments_for(resolve_enum_template(template_info).parameters, instantiation_view);
    Scope instantiation_scope;
    return instantiate_enum_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}

auto libresolve::Context::instantiate_alias_template_with_synthetic_arguments(
    utl::Wrapper<Alias_template_info> const template_info,
    utl::Source_view                  const instantiation_view) -> utl::Wrapper<Alias_info>
{
    auto const arguments = synthetize_arguments_for(resolve_alias_template(template_info).parameters, instantiation_view);
    Scope instantiation_scope;
    return instantiate_alias_template(template_info, arguments, instantiation_view, instantiation_scope, *template_info->home_namespace);
}
