#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>


libresolve::Resolution_constants::Resolution_constants(hir::Node_arena& arena)
    : immut                 { arena.wrap<hir::Mutability::Variant>(hir::Mutability::Concrete { .is_mutable = false }) }
    , mut                   { arena.wrap<hir::Mutability::Variant>(hir::Mutability::Concrete { .is_mutable = true }) }
    , unit_type             { arena.wrap<hir::Type::Variant>(hir::type::Tuple {}) }
    , i8_type               { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::i8) }
    , i16_type              { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::i16) }
    , i32_type              { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::i32) }
    , i64_type              { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::i64) }
    , u8_type               { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::u8) }
    , u16_type              { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::u16) }
    , u32_type              { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::u32) }
    , u64_type              { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Integer::u64) }
    , floating_type         { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Floating {}) }
    , character_type        { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Character {}) }
    , boolean_type          { arena.wrap<hir::Type::Variant>(compiler::built_in_type::Boolean {}) }
    , string_type           { arena.wrap<hir::Type::Variant>(compiler::built_in_type::String {}) }
    , self_placeholder_type { arena.wrap<hir::Type::Variant>(hir::type::Self_placeholder {}) } {}


auto libresolve::Context::error(utl::Source_view const source_view, utl::diagnostics::Message_arguments const& arguments) -> void {
    diagnostics().emit_error(arguments.add_source_view(source_view));
}

auto libresolve::Context::diagnostics() -> utl::diagnostics::Builder& {
    utl::always_assert(compilation_info.get() != nullptr);
    return compilation_info.get()->diagnostics;
}


auto libresolve::Context::fresh_unification_mutability_variable(utl::Source_view const source_view) -> hir::Mutability {
    return hir::Mutability {
        wrap(hir::Mutability::Variant {
            hir::Mutability::Variable {
                .state = node_arena.wrap(hir::Unification_mutability_variable_state { hir::Unification_mutability_variable_state::Unsolved {
                    .tag = hir::Unification_variable_tag { current_unification_variable_tag++.get() }
                }})
            }
        }),
        source_view,
    };
}


auto libresolve::Context::fresh_unification_type_variable_state(
    hir::Unification_type_variable_kind const variable_kind) -> utl::Wrapper<hir::Unification_type_variable_state>
{
    return node_arena.wrap(hir::Unification_type_variable_state { hir::Unification_type_variable_state::Unsolved {
        .tag = hir::Unification_variable_tag { current_unification_variable_tag++.get() },
        .kind = variable_kind,
    }});
}


auto libresolve::Context::fresh_general_unification_type_variable(utl::Source_view const source_view) -> hir::Type {
    return hir::Type {
        wrap_type(hir::type::Unification_variable {
            .state = fresh_unification_type_variable_state(hir::Unification_type_variable_kind::general)
        }),
        source_view,
    };
}
auto libresolve::Context::fresh_integral_unification_type_variable(utl::Source_view const source_view) -> hir::Type {
    return hir::Type {
        wrap_type(hir::type::Unification_variable {
            fresh_unification_type_variable_state(hir::Unification_type_variable_kind::integral)
        }),
        source_view,
    };
}

auto libresolve::Context::fresh_template_parameter_reference_tag() -> hir::Template_parameter_tag {
    return hir::Template_parameter_tag { current_template_parameter_tag++.get() };
}
auto libresolve::Context::fresh_local_variable_tag() -> hir::Local_variable_tag {
    return hir::Local_variable_tag { current_local_variable_tag++.get() };
}


auto libresolve::Context::predefinitions() -> Predefinitions {
    if (predefinitions_value.has_value())
        return *predefinitions_value;
    else
        utl::abort("missing resolution predefinitions");
}


auto libresolve::Context::immut_constant(utl::Source_view const view) -> hir::Mutability { return hir::Mutability { constants.immut, view }; }
auto libresolve::Context::  mut_constant(utl::Source_view const view) -> hir::Mutability { return hir::Mutability { constants.mut, view }; }

auto libresolve::Context::unit_type            (utl::Source_view const view) -> hir::Type { return hir::Type { constants.unit_type,             view }; }
auto libresolve::Context::i8_type              (utl::Source_view const view) -> hir::Type { return hir::Type { constants.i8_type,               view }; }
auto libresolve::Context::i16_type             (utl::Source_view const view) -> hir::Type { return hir::Type { constants.i16_type,              view }; }
auto libresolve::Context::i32_type             (utl::Source_view const view) -> hir::Type { return hir::Type { constants.i32_type,              view }; }
auto libresolve::Context::i64_type             (utl::Source_view const view) -> hir::Type { return hir::Type { constants.i64_type,              view }; }
auto libresolve::Context::u8_type              (utl::Source_view const view) -> hir::Type { return hir::Type { constants.u8_type,               view }; }
auto libresolve::Context::u16_type             (utl::Source_view const view) -> hir::Type { return hir::Type { constants.u16_type,              view }; }
auto libresolve::Context::u32_type             (utl::Source_view const view) -> hir::Type { return hir::Type { constants.u32_type,              view }; }
auto libresolve::Context::u64_type             (utl::Source_view const view) -> hir::Type { return hir::Type { constants.u64_type,              view }; }
auto libresolve::Context::floating_type        (utl::Source_view const view) -> hir::Type { return hir::Type { constants.floating_type,         view }; }
auto libresolve::Context::boolean_type         (utl::Source_view const view) -> hir::Type { return hir::Type { constants.boolean_type,          view }; }
auto libresolve::Context::character_type       (utl::Source_view const view) -> hir::Type { return hir::Type { constants.character_type,        view }; }
auto libresolve::Context::string_type          (utl::Source_view const view) -> hir::Type { return hir::Type { constants.string_type,           view }; }
auto libresolve::Context::self_placeholder_type(utl::Source_view const view) -> hir::Type { return hir::Type { constants.self_placeholder_type, view }; }
auto libresolve::Context::size_type            (utl::Source_view const view) -> hir::Type { return u64_type(view); }


auto libresolve::Context::temporary_placeholder_type(utl::Source_view const view) -> hir::Type {
    return hir::Type { wrap_type(hir::type::Tuple {}), view };
}


auto libresolve::Context::associated_namespace_if(hir::Type const type)
    -> tl::optional<utl::Wrapper<Namespace>>
{
    auto const& value = *type.flattened_value();
    if (auto const* const structure = std::get_if<hir::type::Structure>(&value))
        return resolve_struct(structure->info).associated_namespace;
    else if (auto const* const enumeration = std::get_if<hir::type::Enumeration>(&value))
        return resolve_enum(enumeration->info).associated_namespace;
    else
        return tl::nullopt;
}

auto libresolve::Context::associated_namespace(hir::Type type)
    -> utl::Wrapper<Namespace>
{
    if (tl::optional space = associated_namespace_if(type))
        return *space;
    error(type.source_view(), { std::format("{} does not have an associated namespace", hir::to_string(type)) });
}


namespace {
    template <auto libresolve::Namespace::* table>
    auto add_to_namespace_impl(
        libresolve::Context  & context,
        libresolve::Namespace& space,
        auto const             name,
        auto                   variant,
        auto                   get_name_from_variant) -> void
    {
        if (auto* const existing = (space.*table).find(name.identifier)) {
            context.diagnostics().emit_error({
                .sections = utl::to_vector<utl::diagnostics::Text_section>({
                    {
                        .source_view = std::visit(get_name_from_variant, *existing).source_view,
                        .note        = "Originally defined here",
                        .note_color  = utl::diagnostics::warning_color,
                    },
                    {
                        .source_view = name.source_view,
                        .note        = "Later defined here",
                    }
                }),
                .message = "{} erroneously redefined"_format(name),
            });
        }
        else {
            (space.*table).add_new_or_abort(name.identifier, std::move(variant));
        }
    }
}

auto libresolve::Context::add_to_namespace(Namespace& space, compiler::Name_lower const name, Lower_variant lower) -> void {
    add_to_namespace_impl<&Namespace::lower_table>(*this, space, name, std::move(lower), utl::Overload {
        [](utl::Wrapper<Namespace> const space) { return utl::get(space->name); },
        [](hir::Enum_constructor   const ctor)  { return ctor.name; },
        [](utl::wrapper auto       const info)  { return info->name; }
    });
}
auto libresolve::Context::add_to_namespace(Namespace& space, compiler::Name_upper const name, Upper_variant upper) -> void {
    add_to_namespace_impl<&Namespace::upper_table>(*this, space, name, std::move(upper), [](auto const& info) { return info->name;  });
}


auto libresolve::Context::resolve_mutability(ast::Mutability const& mutability, Scope& scope)
    -> hir::Mutability
{
    return utl::match(mutability.value,
        [&](ast::Mutability::Concrete const concrete) {
            return concrete.is_mutable.get()
                ? mut_constant(mutability.source_view)
                : immut_constant(mutability.source_view);
        },
        [&](ast::Mutability::Parameterized const parameterized) {
            if (auto* const mutability_binding = scope.find_mutability(parameterized.name.identifier)) {
                mutability_binding->has_been_mentioned = true;
                return mutability_binding->mutability.with(mutability.source_view);
            }
            error(mutability.source_view, { "No mutability parameter '{}: mut' in scope"_format(parameterized.name) });
        }
    );
}


auto libresolve::Context::resolve_class_reference(
    ast::Class_reference& reference,
    Scope               & scope,
    Namespace           & space) -> hir::Class_reference
{
    return utl::match(find_upper(reference.name, scope, space),
        [&](utl::Wrapper<Typeclass_info> info) -> hir::Class_reference {
            return { .info = info, .source_view = reference.source_view };
        },
        [&, this](auto const&) -> hir::Class_reference {
            error(reference.source_view, { "Not a typeclass" }); // TODO: improve message
        }
    );
}


auto libresolve::Context::resolve_template_parameters(
    std::span<ast::Template_parameter> const ast_parameters,
    Namespace                              & space) -> utl::Pair<Scope, std::vector<hir::Template_parameter>>
{
    Scope parameter_scope;
    auto  parameters = utl::vector_with_capacity<hir::Template_parameter>(ast_parameters.size());

    for (ast::Template_parameter& parameter : ast_parameters) {
        auto const reference_tag = fresh_template_parameter_reference_tag();

        auto const add_parameter = [&](hir::Template_parameter::Variant&& value) {
            auto const make_default_argument = [&](ast::Template_argument&& default_argument) {
                // Validate the default argument by resolving it here, but discard the result
                utl::match(default_argument.value,
                        [&](utl::Wrapper<ast::Type>       const type_argument)       { (void)resolve_type(*type_argument, parameter_scope, space); },
                        [&](utl::Wrapper<ast::Expression> const value_argument)      { (void)resolve_expression(*value_argument, parameter_scope, space); },
                        [&](ast::Mutability               const mutability_argument) { (void)resolve_mutability(mutability_argument, parameter_scope); },
                        [&](ast::Template_argument::Wildcard) {});
                return hir::Template_default_argument {
                    .argument = std::move(default_argument),
                    .scope    = std::make_unique<Scope>(parameter_scope),
                };
            };
            parameters.push_back(hir::Template_parameter {
                .value            = std::move(value),
                .default_argument = std::move(parameter.default_argument).transform(make_default_argument),
                .reference_tag    = reference_tag,
                .source_view      = parameter.source_view,
            });
        };

        utl::match(parameter.value,
            [&](ast::Template_parameter::Type_parameter& type_parameter) {
                parameter_scope.bind_type(*this, type_parameter.name.identifier, {
                    .type = hir::Type {
                        wrap_type(hir::type::Template_parameter_reference {
                            .identifier = { type_parameter.name.identifier },
                            .tag        = reference_tag,
                        }),
                        type_parameter.name.source_view,
                    },
                    .source_view = parameter.source_view,
                });
                auto const resolve_class = [&](ast::Class_reference& reference) {
                    return resolve_class_reference(reference, parameter_scope, space);
                };
                add_parameter(hir::Template_parameter::Type_parameter {
                    .classes = utl::map(resolve_class, type_parameter.classes)
                });
            },
            [&](ast::Template_parameter::Mutability_parameter& mutability_parameter) {
                parameter_scope.bind_mutability(*this, mutability_parameter.name.identifier, {
                    .mutability = hir::Mutability {
                        wrap(hir::Mutability::Variant {
                            hir::Mutability::Parameterized {
                                .identifier = mutability_parameter.name.identifier,
                                .tag        = reference_tag,
                            }
                        }),
                        parameter.source_view,
                    },
                    .source_view = parameter.source_view,
                });
                add_parameter(hir::Template_parameter::Mutability_parameter {
                    .name = mutability_parameter.name,
                });
            },
            [](ast::Template_parameter::Value_parameter&) {
                utl::todo();
            }
        );
    }

    return { std::move(parameter_scope), std::move(parameters) };
}


auto libresolve::Enum_info::constructor_count() const noexcept -> utl::Usize {
    return utl::match(value, [](auto const& enumeration) { return enumeration.constructors.size(); });
}
