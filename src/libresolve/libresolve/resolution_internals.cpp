#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>


libresolve::Definition_state_guard::Definition_state_guard(
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

libresolve::Definition_state_guard::~Definition_state_guard() {
    // If the destructor is called due to an uncaught exception
    // in definition resolution code, don't modify the state.

    if (std::uncaught_exceptions() == initial_exception_count)
        definition_state = Definition_state::resolved;
}


libresolve::Self_type_guard::Self_type_guard(Context& context, mir::Type new_self_type)
    : current_self_type  { context.current_self_type }
    , previous_self_type { current_self_type }
{
    current_self_type = new_self_type;
}

libresolve::Self_type_guard::~Self_type_guard() {
    current_self_type = previous_self_type;
}


libresolve::Resolution_constants::Resolution_constants(mir::Node_arena& arena)
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


auto libresolve::Context::error(utl::Source_view const source_view, utl::diagnostics::Message_arguments const& arguments) -> void {
    diagnostics().emit_error(arguments.add_source_view(source_view));
}

auto libresolve::Context::diagnostics() -> utl::diagnostics::Builder& {
    utl::always_assert(compilation_info.get() != nullptr);
    return compilation_info.get()->diagnostics;
}


auto libresolve::Context::fresh_unification_mutability_variable(utl::Source_view const source_view) -> mir::Mutability {
    return mir::Mutability {
        wrap(mir::Mutability::Variant {
            mir::Mutability::Variable {
                .state = node_arena.wrap(mir::Unification_mutability_variable_state { mir::Unification_mutability_variable_state::Unsolved {
                    .tag = mir::Unification_variable_tag { current_unification_variable_tag++.get() }
                }})
            }
        }),
        source_view,
    };
}


auto libresolve::Context::fresh_unification_type_variable_state(
    mir::Unification_type_variable_kind const variable_kind) -> utl::Wrapper<mir::Unification_type_variable_state>
{
    return node_arena.wrap(mir::Unification_type_variable_state { mir::Unification_type_variable_state::Unsolved {
        .tag = mir::Unification_variable_tag { current_unification_variable_tag++.get() },
        .kind = variable_kind,
    }});
}


auto libresolve::Context::fresh_general_unification_type_variable(utl::Source_view const source_view) -> mir::Type {
    return mir::Type {
        wrap_type(mir::type::Unification_variable {
            .state = fresh_unification_type_variable_state(mir::Unification_type_variable_kind::general)
        }),
        source_view,
    };
}
auto libresolve::Context::fresh_integral_unification_type_variable(utl::Source_view const source_view) -> mir::Type {
    return mir::Type {
        wrap_type(mir::type::Unification_variable {
            fresh_unification_type_variable_state(mir::Unification_type_variable_kind::integral)
        }),
        source_view,
    };
}

auto libresolve::Context::fresh_template_parameter_reference_tag() -> mir::Template_parameter_tag {
    return mir::Template_parameter_tag { current_template_parameter_tag++.get() };
}
auto libresolve::Context::fresh_local_variable_tag() -> mir::Local_variable_tag {
    return mir::Local_variable_tag { current_local_variable_tag++.get() };
}


auto libresolve::Context::predefinitions() -> Predefinitions {
    if (predefinitions_value.has_value())
        return *predefinitions_value;
    else
        utl::abort("missing resolution predefinitions");
}


auto libresolve::Context::immut_constant(utl::Source_view const view) -> mir::Mutability { return mir::Mutability { constants.immut, view }; }
auto libresolve::Context::  mut_constant(utl::Source_view const view) -> mir::Mutability { return mir::Mutability { constants.mut, view }; }

auto libresolve::Context::unit_type            (utl::Source_view const view) -> mir::Type { return mir::Type { constants.unit_type,             view }; }
auto libresolve::Context::i8_type              (utl::Source_view const view) -> mir::Type { return mir::Type { constants.i8_type,               view }; }
auto libresolve::Context::i16_type             (utl::Source_view const view) -> mir::Type { return mir::Type { constants.i16_type,              view }; }
auto libresolve::Context::i32_type             (utl::Source_view const view) -> mir::Type { return mir::Type { constants.i32_type,              view }; }
auto libresolve::Context::i64_type             (utl::Source_view const view) -> mir::Type { return mir::Type { constants.i64_type,              view }; }
auto libresolve::Context::u8_type              (utl::Source_view const view) -> mir::Type { return mir::Type { constants.u8_type,               view }; }
auto libresolve::Context::u16_type             (utl::Source_view const view) -> mir::Type { return mir::Type { constants.u16_type,              view }; }
auto libresolve::Context::u32_type             (utl::Source_view const view) -> mir::Type { return mir::Type { constants.u32_type,              view }; }
auto libresolve::Context::u64_type             (utl::Source_view const view) -> mir::Type { return mir::Type { constants.u64_type,              view }; }
auto libresolve::Context::floating_type        (utl::Source_view const view) -> mir::Type { return mir::Type { constants.floating_type,         view }; }
auto libresolve::Context::boolean_type         (utl::Source_view const view) -> mir::Type { return mir::Type { constants.boolean_type,          view }; }
auto libresolve::Context::character_type       (utl::Source_view const view) -> mir::Type { return mir::Type { constants.character_type,        view }; }
auto libresolve::Context::string_type          (utl::Source_view const view) -> mir::Type { return mir::Type { constants.string_type,           view }; }
auto libresolve::Context::self_placeholder_type(utl::Source_view const view) -> mir::Type { return mir::Type { constants.self_placeholder_type, view }; }
auto libresolve::Context::size_type            (utl::Source_view const view) -> mir::Type { return u64_type(view); }


auto libresolve::Context::temporary_placeholder_type(utl::Source_view const view) -> mir::Type {
    return mir::Type { wrap_type(mir::type::Tuple {}), view };
}


auto libresolve::Context::associated_namespace_if(mir::Type const type)
    -> tl::optional<utl::Wrapper<Namespace>>
{
    auto const& value = *type.flattened_value();
    if (auto const* const structure = std::get_if<mir::type::Structure>(&value))
        return resolve_struct(structure->info).associated_namespace;
    else if (auto const* const enumeration = std::get_if<mir::type::Enumeration>(&value))
        return resolve_enum(enumeration->info).associated_namespace;
    else
        return tl::nullopt;
}

auto libresolve::Context::associated_namespace(mir::Type type)
    -> utl::Wrapper<Namespace>
{
    if (tl::optional space = associated_namespace_if(type))
        return *space;
    error(type.source_view(), { "{} does not have an associated namespace"_format(type) });
}


namespace {
    template <auto libresolve::Namespace::* table>
    auto add_to_namespace_impl(
        libresolve::Context  & context,
        libresolve::Namespace& space,
        ast::Name        const name,
        auto                   variant,
        auto                   get_name_from_variant) -> void
    {
        if (auto* const existing = (space.*table).find(name.identifier)) {
            context.diagnostics().emit_error({
                .sections = utl::to_vector<utl::diagnostics::Text_section>({
                    {
                        .source_view = std::visit(get_name_from_variant, *existing).source_view,
                        .note        = "Originally defined here",
                        .note_color  = utl::diagnostics::warning_color
                    },
                    {
                        .source_view = name.source_view,
                        .note        = "Later defined here"
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

auto libresolve::Context::add_to_namespace(Namespace& space, ast::Name const name, Lower_variant lower) -> void {
    assert(!name.is_upper.get());
    add_to_namespace_impl<&Namespace::lower_table>(*this, space, name, std::move(lower), utl::Overload {
        [](utl::Wrapper<Namespace> const space) { return utl::get(space->name); },
        [](mir::Enum_constructor   const ctor)  { return ctor.name; },
        [](utl::wrapper auto       const info)  { return info->name; }
    });
}
auto libresolve::Context::add_to_namespace(Namespace& space, ast::Name const name, Upper_variant upper) -> void {
    assert(name.is_upper.get());
    add_to_namespace_impl<&Namespace::upper_table>(*this, space, name, std::move(upper), [](auto const& info) { return info->name;  });
}


auto libresolve::Context::resolve_mutability(ast::Mutability const mutability, Scope& scope)
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
            error(mutability.source_view, { "No mutability parameter '{}: mut' in scope"_format(parameterized.identifier) });
        }
    );
}


auto libresolve::Context::resolve_class_reference(
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


auto libresolve::Context::resolve_template_parameters(
    std::span<hir::Template_parameter> const hir_parameters,
    Namespace                              & space) -> utl::Pair<Scope, std::vector<mir::Template_parameter>>
{
    Scope parameter_scope;
    auto  parameters = utl::vector_with_capacity<mir::Template_parameter>(hir_parameters.size());

    for (hir::Template_parameter& parameter : hir_parameters) {
        auto const reference_tag = fresh_template_parameter_reference_tag();

        auto const add_parameter = [&](mir::Template_parameter::Variant&& value) {
            auto const make_default_argument = [&](hir::Template_argument&& default_argument) {
                // Validate the default argument by resolving it here, but discard the result
                utl::match(default_argument.value,
                        [&](utl::Wrapper<hir::Type>       const type_argument)       { (void)resolve_type(*type_argument, parameter_scope, space); },
                        [&](utl::Wrapper<hir::Expression> const value_argument)      { (void)resolve_expression(*value_argument, parameter_scope, space); },
                        [&](ast::Mutability               const mutability_argument) { (void)resolve_mutability(mutability_argument, parameter_scope); },
                        [&](hir::Template_argument::Wildcard) {});
                return mir::Template_default_argument {
                    .argument = std::move(default_argument),
                    .scope    = std::make_unique<Scope>(parameter_scope),
                };
            };
            parameters.push_back(mir::Template_parameter {
                .value            = std::move(value),
                .name             = { parameter.name },
                .default_argument = std::move(parameter.default_argument).transform(make_default_argument),
                .reference_tag    = reference_tag,
                .source_view      = parameter.source_view,
            });
        };

        utl::match(parameter.value,
            [&](hir::Template_parameter::Type_parameter& type_parameter) {
                parameter_scope.bind_type(*this, parameter.name.identifier, {
                    .type = mir::Type {
                        wrap_type(mir::type::Template_parameter_reference {
                            .identifier = { parameter.name.identifier },
                            .tag        = reference_tag,
                        }),
                        parameter.name.source_view,
                    },
                    .source_view = parameter.source_view,
                });
                auto const resolve_class = [&](hir::Class_reference& reference) {
                    return resolve_class_reference(reference, parameter_scope, space);
                };
                add_parameter(mir::Template_parameter::Type_parameter {
                    .classes = utl::map(resolve_class, type_parameter.classes)
                });
            },
            [&](hir::Template_parameter::Mutability_parameter&) {
                parameter_scope.bind_mutability(*this, parameter.name.identifier, {
                    .mutability = mir::Mutability {
                        wrap(mir::Mutability::Variant {
                            mir::Mutability::Parameterized {
                                .identifier = parameter.name.identifier,
                                .tag        = reference_tag,
                            }
                        }),
                        parameter.name.source_view,
                    },
                    .source_view = parameter.source_view,
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


auto libresolve::Enum_info::constructor_count() const noexcept -> utl::Usize {
    return utl::match(value, [](auto const& enumeration) { return enumeration.constructors.size(); });
}


DEFINE_FORMATTER_FOR(libresolve::constraint::Type_equality) {
    return fmt::format_to(context.out(), "{} ~ {}", value.constrainer_type, value.constrained_type);
}
DEFINE_FORMATTER_FOR(libresolve::constraint::Mutability_equality) {
    return fmt::format_to(context.out(), "{} ~ {}", value.constrainer_mutability, value.constrained_mutability);
}
DEFINE_FORMATTER_FOR(libresolve::constraint::Instance) {
    return fmt::format_to(context.out(), "{}: {}", value.type, value.typeclass->name);
}
DEFINE_FORMATTER_FOR(libresolve::constraint::Struct_field) {
    return fmt::format_to(context.out(), "({}.{}): {}", value.struct_type, value.field_identifier, value.field_type);
}
DEFINE_FORMATTER_FOR(libresolve::constraint::Tuple_field) {
    return fmt::format_to(context.out(), "({}.{}): {}", value.tuple_type, value.field_index, value.field_type);
}
