#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


resolution::Definition_state_guard::Definition_state_guard(
    Context         & context,
    Definition_state& state,
    ast::Name   const name)
    : definition_state        { state }
    , initial_exception_count { std::uncaught_exceptions() }
{
    if (state == Definition_state::currently_on_resolution_stack)
        context.error(name.source_view, { "Unable to resolve circular dependency" });
    else
        state = Definition_state::currently_on_resolution_stack;
}

resolution::Definition_state_guard::~Definition_state_guard() {
    // If the destructor is called due to an uncaught exception
    // in definition resolution code, don't modify the state.

    if (std::uncaught_exceptions() == initial_exception_count)
        definition_state = Definition_state::resolved;
}


resolution::Self_type_guard::Self_type_guard(Context& context, mir::Type new_self_type)
    : current_self_type  { context.current_self_type }
    , previous_self_type { current_self_type }
{
    current_self_type = new_self_type;
}

resolution::Self_type_guard::~Self_type_guard() {
    current_self_type = previous_self_type;
}


resolution::Resolution_constants::Resolution_constants(mir::Node_arena& arena)
    : immut                 { arena.wrap<mir::Mutability::Variant>(mir::Mutability::Concrete { .is_mutable = false }) }
    , mut                   { arena.wrap<mir::Mutability::Variant>(mir::Mutability::Concrete { .is_mutable = true }) }
    , unit_type             { arena.wrap<mir::Type::Variant>(mir::type::Tuple {}) }
    , i8_type               { arena.wrap<mir::Type::Variant>(mir::type::Integer::i8) }
    , i16_type              { arena.wrap<mir::Type::Variant>(mir::type::Integer::i16) }
    , i32_type              { arena.wrap<mir::Type::Variant>(mir::type::Integer::i32) }
    , i64_type              { arena.wrap<mir::Type::Variant>(mir::type::Integer::i64) }
    , u8_type               { arena.wrap<mir::Type::Variant>(mir::type::Integer::u8) }
    , u16_type              { arena.wrap<mir::Type::Variant>(mir::type::Integer::u16) }
    , u32_type              { arena.wrap<mir::Type::Variant>(mir::type::Integer::u32) }
    , u64_type              { arena.wrap<mir::Type::Variant>(mir::type::Integer::u64) }
    , floating_type         { arena.wrap<mir::Type::Variant>(mir::type::Floating {}) }
    , character_type        { arena.wrap<mir::Type::Variant>(mir::type::Character {}) }
    , boolean_type          { arena.wrap<mir::Type::Variant>(mir::type::Boolean {}) }
    , string_type           { arena.wrap<mir::Type::Variant>(mir::type::String {}) }
    , self_placeholder_type { arena.wrap<mir::Type::Variant>(mir::type::Self_placeholder {}) } {}


auto resolution::Context::error(
    utl::Source_view                    const source_view,
    utl::diagnostics::Message_arguments const arguments) -> void
{
    compilation_info.diagnostics.emit_simple_error(arguments.add_source_info(source, source_view));
}


auto resolution::Context::fresh_unification_mutability_variable(utl::Source_view const view) -> mir::Mutability {
    return {
        .value = wrap(mir::Mutability::Variant {
            mir::Mutability::Variable {
                .tag = mir::Unification_variable_tag { current_unification_variable_tag++.get() }
            }
        }),
        .source_view = view
    };
}

auto resolution::Context::fresh_general_unification_type_variable(utl::Source_view const view) -> mir::Type {
    return {
        .value = wrap_type(mir::type::General_unification_variable {
            .tag = mir::Unification_variable_tag { current_unification_variable_tag++.get() }
        }),
        .source_view = view
    };
}
auto resolution::Context::fresh_integral_unification_type_variable(utl::Source_view const view) -> mir::Type {
    return {
        .value = wrap_type(mir::type::Integral_unification_variable {
            .tag = mir::Unification_variable_tag { current_unification_variable_tag++.get() }
        }),
        .source_view = view
    };
}

auto resolution::Context::fresh_template_parameter_reference_tag() -> mir::Template_parameter_tag {
    return mir::Template_parameter_tag { current_template_parameter_tag++.get() };
}
auto resolution::Context::fresh_local_variable_tag() -> mir::Local_variable_tag {
    return mir::Local_variable_tag { current_local_variable_tag++.get() };
}


auto resolution::Context::immut_constant(utl::Source_view const view) -> mir::Mutability { return { constants.immut, view }; }
auto resolution::Context::  mut_constant(utl::Source_view const view) -> mir::Mutability { return { constants.mut, view }; }

auto resolution::Context::unit_type            (utl::Source_view const view) -> mir::Type { return { constants.unit_type,             view }; }
auto resolution::Context::i8_type              (utl::Source_view const view) -> mir::Type { return { constants.i8_type,               view }; }
auto resolution::Context::i16_type             (utl::Source_view const view) -> mir::Type { return { constants.i16_type,              view }; }
auto resolution::Context::i32_type             (utl::Source_view const view) -> mir::Type { return { constants.i32_type,              view }; }
auto resolution::Context::i64_type             (utl::Source_view const view) -> mir::Type { return { constants.i64_type,              view }; }
auto resolution::Context::u8_type              (utl::Source_view const view) -> mir::Type { return { constants.u8_type,               view }; }
auto resolution::Context::u16_type             (utl::Source_view const view) -> mir::Type { return { constants.u16_type,              view }; }
auto resolution::Context::u32_type             (utl::Source_view const view) -> mir::Type { return { constants.u32_type,              view }; }
auto resolution::Context::u64_type             (utl::Source_view const view) -> mir::Type { return { constants.u64_type,              view }; }
auto resolution::Context::floating_type        (utl::Source_view const view) -> mir::Type { return { constants.floating_type,         view }; }
auto resolution::Context::boolean_type         (utl::Source_view const view) -> mir::Type { return { constants.boolean_type,          view }; }
auto resolution::Context::character_type       (utl::Source_view const view) -> mir::Type { return { constants.character_type,        view }; }
auto resolution::Context::string_type          (utl::Source_view const view) -> mir::Type { return { constants.string_type,           view }; }
auto resolution::Context::self_placeholder_type(utl::Source_view const view) -> mir::Type { return { constants.self_placeholder_type, view }; }
auto resolution::Context::size_type            (utl::Source_view const view) -> mir::Type { return u64_type(view); }


auto resolution::Context::temporary_placeholder_type(utl::Source_view const view) -> mir::Type {
    return {
        .value       = wrap_type(mir::type::Tuple {}),
        .source_view = view
    };
}


auto resolution::Context::associated_namespace_if(mir::Type type)
    -> tl::optional<utl::Wrapper<Namespace>>
{
    if (auto* const structure = std::get_if<mir::type::Structure>(&*type.value))
        return resolve_struct(structure->info).associated_namespace;
    else if (auto* const enumeration = std::get_if<mir::type::Enumeration>(&*type.value))
        return resolve_enum(enumeration->info).associated_namespace;
    else
        return tl::nullopt;
}

auto resolution::Context::associated_namespace(mir::Type type)
    -> utl::Wrapper<Namespace>
{
    if (tl::optional space = associated_namespace_if(type))
        return *space;
    error(type.source_view, {
        .message           = "{} does not have an associated namespace",
        .message_arguments = fmt::make_format_args(type)
    });
}


namespace {
    template <auto resolution::Namespace::* table>
    auto add_to_namespace_impl(
        resolution::Context  & context,
        resolution::Namespace& space,
        ast::Name        const name,
        auto                   variant,
        auto                   get_name_from_variant) -> void
    {
        if (auto* const existing = (space.*table).find(name.identifier)) {
            context.compilation_info.diagnostics.emit_error({
                .sections = utl::to_vector<utl::diagnostics::Text_section>({
                    {
                        .source_view = std::visit(get_name_from_variant, *existing).source_view,
                        .source      = context.source,
                        .note        = "Originally defined here",
                        .note_color  = utl::diagnostics::warning_color
                    },
                    {
                        .source_view = name.source_view,
                        .source      = context.source,
                        .note        = "Later defined here"
                    }
                }),
                .message           = "{} erroneously redefined",
                .message_arguments = fmt::make_format_args(name)
            });
        }
        else {
            (space.*table).add_new_or_abort(name.identifier, std::move(variant));
        }
    }
}

auto resolution::Context::add_to_namespace(Namespace& space, ast::Name const name, Lower_variant lower) -> void {
    assert(!name.is_upper);
    add_to_namespace_impl<&Namespace::lower_table>(*this, space, name, std::move(lower), utl::Overload {
        [](utl::Wrapper<Namespace> const space) { return utl::get(space->name); },
        [](mir::Enum_constructor   const ctor)  { return ctor.name; },
        [](utl::wrapper auto       const info)  { return info->name; }
    });
}
auto resolution::Context::add_to_namespace(Namespace& space, ast::Name const name, Upper_variant upper) -> void {
    assert(name.is_upper);
    add_to_namespace_impl<&Namespace::upper_table>(*this, space, name, std::move(upper), [](auto const& info) { return info->name;  });
}


auto resolution::Context::resolve_mutability(ast::Mutability const mutability, Scope& scope)
    -> mir::Mutability
{
    return utl::match(mutability.value,
        [&](ast::Mutability::Concrete const concrete) {
            return concrete.is_mutable
                ? mut_constant(mutability.source_view)
                : immut_constant(mutability.source_view);
        },
        [&](ast::Mutability::Parameterized const parameterized) {
            if (auto* const mutability_binding = scope.find_mutability(parameterized.identifier)) {
                mutability_binding->has_been_mentioned = true;
                return mutability_binding->mutability.with(mutability.source_view);
            }
            else {
                error(mutability.source_view, {
                    .message           = "No mutability parameter '{}' in scope",
                    .message_arguments = fmt::make_format_args(parameterized.identifier)
                });
            }
        }
    );
}


auto resolution::Context::resolve_class_reference(
    hir::Class_reference& reference,
    Scope               & scope,
    Namespace           & space) -> mir::Class_reference
{
    return utl::match(find_upper(reference.name, scope, space),
        [&](utl::Wrapper<Typeclass_info> info) -> mir::Class_reference {
            return { .info = info, .source_view = reference.source_view };
        },
        [&, this](auto const&) -> mir::Class_reference {
            error(reference.source_view, { "Not a typeclass" }); // TODO: improve message
        }
    );
}


auto resolution::Context::resolve_template_parameters(
    std::span<hir::Template_parameter> const hir_parameters,
    Namespace                              & space) -> utl::Pair<Scope, std::vector<mir::Template_parameter>>
{
    Scope parameter_scope { *this };
    auto  parameters = utl::vector_with_capacity<mir::Template_parameter>(hir_parameters.size());

    auto const resolve_default_argument = [&](mir::Template_parameter::Variant const& parameter_value, hir::Template_argument const& argument) {
        return mir::Template_argument {
            .value = std::visit<mir::Template_argument::Variant>(utl::Overload {
                [&](mir::Template_parameter::Type_parameter const& type_parameter, utl::Wrapper<hir::Type> const type_argument) {
                    if (!type_parameter.classes.empty()) { utl::todo(); }
                    return resolve_type(*type_argument, parameter_scope, space);
                },
                [&](mir::Template_parameter::Mutability_parameter const&, ast::Mutability const mutability) {
                    return resolve_mutability(mutability, parameter_scope);
                },
                [&](mir::Template_parameter::Type_parameter const& type_parameter, hir::Template_argument::Wildcard const wildcard) {
                    if (!type_parameter.classes.empty()) { utl::todo(); }
                    return fresh_general_unification_type_variable(wildcard.source_view);
                },
                [&](mir::Template_parameter::Mutability_parameter, hir::Template_argument::Wildcard const wildcard) {
                    return fresh_unification_mutability_variable(wildcard.source_view);
                },
                [&](auto const&, auto const&) -> mir::Template_argument::Variant {
                    utl::todo();
                }
            }, parameter_value, argument.value),
            .name = argument.name
        };
    };

    for (hir::Template_parameter& parameter : hir_parameters) {
        auto const reference_tag = fresh_template_parameter_reference_tag();

        auto const add_parameter = [&](mir::Template_parameter::Variant&& value) {
            auto default_argument = parameter.default_argument.transform(std::bind_front(resolve_default_argument, std::cref(value)));

            parameters.push_back(mir::Template_parameter {
                .value            = std::move(value),
                .name             = parameter.name,
                .default_argument = std::move(default_argument),
                .reference_tag    = reference_tag,
                .source_view      = parameter.source_view,
            });
        };

        utl::match(parameter.value,
            [&](hir::Template_parameter::Type_parameter& type_parameter) {
                parameter_scope.bind_type(parameter.name.identifier, {
                    .type {
                        .value = wrap_type(mir::type::Template_parameter_reference {
                            .identifier = parameter.name.identifier,
                            .tag        = reference_tag
                        }),
                        .source_view = parameter.name.source_view
                    },
                    .source_view = parameter.source_view
                });
                auto const resolve_class = [&](hir::Class_reference& reference) {
                    return resolve_class_reference(reference, parameter_scope, space);
                };
                add_parameter(mir::Template_parameter::Type_parameter {
                    .classes = utl::map(resolve_class, type_parameter.classes)
                });
            },
            [&](hir::Template_parameter::Mutability_parameter&) {
                parameter_scope.bind_mutability(parameter.name.identifier, {
                    .mutability {
                        .value = wrap(mir::Mutability::Variant {
                            mir::Mutability::Parameterized {
                                .identifier = parameter.name.identifier,
                                .tag        = reference_tag
                            }
                        }),
                        .source_view = parameter.name.source_view
                    },
                    .source_view = parameter.source_view
                });
                add_parameter(mir::Template_parameter::Mutability_parameter {});
            },
            [](hir::Template_parameter::Value_parameter&) {
                utl::todo();
            }
        );
    }

    return { std::move(parameter_scope), std::move(parameters) };
}


auto resolution::Enum_info::constructor_count() const noexcept -> utl::Usize {
    return std::visit([](auto const& enumeration) { return enumeration.constructors.size(); }, value);
}


DEFINE_FORMATTER_FOR(resolution::constraint::Type_equality) {
    return fmt::format_to(context.out(), "{} ~ {}", value.constrainer_type, value.constrained_type);
}

DEFINE_FORMATTER_FOR(resolution::constraint::Mutability_equality) {
    return fmt::format_to(context.out(), "{} ~ {}", value.constrainer_mutability, value.constrained_mutability);
}

DEFINE_FORMATTER_FOR(resolution::constraint::Instance) {
    return fmt::format_to(context.out(), "{}: {}", value.type, value.typeclass->name);
}

DEFINE_FORMATTER_FOR(resolution::constraint::Struct_field) {
    return fmt::format_to(context.out(), "({}.{}): {}", value.struct_type, value.field_identifier, value.field_type);
}

DEFINE_FORMATTER_FOR(resolution::constraint::Tuple_field) {
    return fmt::format_to(context.out(), "({}.{}): {}", value.tuple_type, value.field_index, value.field_type);
}
