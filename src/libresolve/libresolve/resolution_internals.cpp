#include <libutl/safe_integer.hpp>
#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::Arenas::defaults() -> Arenas
{
    return Arenas {
        .info_arena        = Info_arena::with_default_page_size(),
        .environment_arena = Environment_arena::with_default_page_size(),
        .ast_node_arena    = ast::Node_arena::with_default_page_size(),
        .hir_node_arena    = hir::Node_arena::with_default_page_size(),
    };
}

auto libresolve::Arenas::type(hir::Type::Variant variant)
    -> utl::Mutable_wrapper<hir::Type::Variant>
{
    return hir_node_arena.wrap_mutable<hir::Type::Variant>(std::move(variant));
}

auto libresolve::Arenas::mutability(hir::Mutability::Variant variant)
    -> utl::Mutable_wrapper<hir::Mutability::Variant>
{
    return hir_node_arena.wrap_mutable<hir::Mutability::Variant>(std::move(variant));
}

auto libresolve::Arenas::wrap(hir::Expression expression) -> utl::Wrapper<hir::Expression>
{
    return hir_node_arena.wrap<hir::Expression>(std::move(expression));
}

auto libresolve::Arenas::wrap(hir::Pattern pattern) -> utl::Wrapper<hir::Pattern>
{
    return hir_node_arena.wrap<hir::Pattern>(std::move(pattern));
}

auto libresolve::Constants::make_with(Arenas& arenas) -> Constants
{
    return Constants {
        .i8_type          = arenas.type(kieli::built_in_type::Integer::i8),
        .i16_type         = arenas.type(kieli::built_in_type::Integer::i16),
        .i32_type         = arenas.type(kieli::built_in_type::Integer::i32),
        .i64_type         = arenas.type(kieli::built_in_type::Integer::i64),
        .u8_type          = arenas.type(kieli::built_in_type::Integer::u8),
        .u16_type         = arenas.type(kieli::built_in_type::Integer::u16),
        .u32_type         = arenas.type(kieli::built_in_type::Integer::u32),
        .u64_type         = arenas.type(kieli::built_in_type::Integer::u64),
        .boolean_type     = arenas.type(kieli::built_in_type::Boolean {}),
        .floating_type    = arenas.type(kieli::built_in_type::Floating {}),
        .string_type      = arenas.type(kieli::built_in_type::String {}),
        .character_type   = arenas.type(kieli::built_in_type::Character {}),
        .unit_type        = arenas.type(hir::type::Tuple {}),
        .error_type       = arenas.type(hir::type::Error {}),
        .mutability_yes   = arenas.mutability(hir::mutability::Concrete::mut),
        .mutability_no    = arenas.mutability(hir::mutability::Concrete::immut),
        .mutability_error = arenas.mutability(hir::mutability::Error {}),
    };
}

auto libresolve::Tag_state::fresh_template_parameter_tag() -> Template_parameter_tag
{
    cpputil::always_assert(!utl::would_increment_overflow(m_current_template_parameter_tag));
    return Template_parameter_tag { ++m_current_template_parameter_tag };
}

auto libresolve::Tag_state::fresh_local_variable_tag() -> Local_variable_tag
{
    cpputil::always_assert(!utl::would_increment_overflow(m_current_local_variable_tag));
    return Local_variable_tag { ++m_current_local_variable_tag };
}

auto libresolve::error_expression(Constants const& constants, utl::Source_range const source_range)
    -> hir::Expression
{
    return hir::Expression {
        hir::expression::Error {},
        hir::Type { constants.error_type, source_range },
        hir::Expression_kind::place,
        source_range,
    };
}

auto libresolve::unit_expression(Constants const& constants, utl::Source_range const source_range)
    -> hir::Expression
{
    return hir::Expression {
        hir::expression::Tuple {},
        hir::Type { constants.unit_type, source_range },
        hir::Expression_kind::value,
        source_range,
    };
}

auto libresolve::Inference_state::flatten(hir::Type::Variant& type) -> void
{
    if (auto const* const variable = std::get_if<hir::type::Variable>(&type)) {
        Type_variable_data& data = type_variables[variable->tag];
        assert(variable->tag == data.tag);
        if (data.is_solved) {
            type           = *data.variant;
            data.is_solved = true;
            return;
        }
        std::size_t const index = type_variable_disjoint_set.find(data.tag.get());
        if (data.tag.get() == index) {
            return;
        }
        Type_variable_data& representative = type_variables.underlying.at(index);
        flatten(representative.variant.as_mutable());
        if (representative.is_solved) {
            type           = *representative.variant;
            data.is_solved = true;
            return;
        }
    }
}

auto libresolve::Inference_state::set_solution(
    kieli::Diagnostics& diagnostics,
    Type_variable_data& variable_data,
    hir::Type::Variant  solution) -> void
{
    std::size_t const   index          = type_variable_disjoint_set.find(variable_data.tag.get());
    Type_variable_data& representative = type_variables.underlying.at(index);
    if (representative.is_solved) {
        require_subtype_relationship(diagnostics, *this, solution, *representative.variant);
    }
    representative.variant.as_mutable() = std::move(solution);
    representative.is_solved            = true;
}

auto libresolve::Inference_state::set_solution(
    kieli::Diagnostics& /*diagnostics*/,
    Mutability_variable_data& variable_data,
    hir::Mutability::Variant  solution) -> void
{
    std::size_t const index = mutability_variable_disjoint_set.find(variable_data.tag.get());
    Mutability_variable_data& representative = mutability_variables.underlying.at(index);
    cpputil::always_assert(!std::exchange(representative.is_solved, true));
    representative.variant.as_mutable() = std::move(solution);
}

auto libresolve::Inference_state::fresh_general_type_variable(
    Arenas& arenas, utl::Source_range const origin) -> hir::Type
{
    auto const tag = Type_variable_tag { type_variables.underlying.size() };

    auto const variant = arenas.type(hir::type::Variable { tag });
    type_variables.underlying.push_back(Type_variable_data {
        .tag     = tag,
        .kind    = hir::Type_variable_kind::general,
        .origin  = origin,
        .variant = variant,
    });
    (void)type_variable_disjoint_set.add();
    return hir::Type { variant, origin };
}

auto libresolve::Inference_state::fresh_integral_type_variable(
    Arenas& arenas, utl::Source_range const origin) -> hir::Type
{
    auto const tag     = Type_variable_tag { type_variables.underlying.size() };
    auto const variant = arenas.type(hir::type::Variable { tag });
    type_variables.underlying.push_back(Type_variable_data {
        .tag     = tag,
        .kind    = hir::Type_variable_kind::integral,
        .origin  = origin,
        .variant = variant,
    });
    (void)type_variable_disjoint_set.add();
    return hir::Type { variant, origin };
}

auto libresolve::Inference_state::fresh_mutability_variable(
    Arenas& arenas, utl::Source_range const origin) -> hir::Mutability
{
    auto const tag     = Mutability_variable_tag { mutability_variables.underlying.size() };
    auto const variant = arenas.mutability(hir::mutability::Variable { tag });
    mutability_variables.underlying.push_back(Mutability_variable_data {
        .tag     = tag,
        .origin  = origin,
        .variant = variant,
    });
    (void)mutability_variable_disjoint_set.add();
    return hir::Mutability { variant, origin };
}

auto libresolve::ensure_no_unsolved_variables(
    Inference_state& state, kieli::Diagnostics& diagnostics) -> void
{
    for (Mutability_variable_data& data : state.mutability_variables.underlying) {
        if (!data.is_solved) {
            state.set_solution(diagnostics, data, hir::mutability::Concrete::immut);
        }
    }
    for (Type_variable_data& data : state.type_variables.underlying) {
        state.flatten(data.variant.as_mutable());
        if (!data.is_solved) {
            diagnostics.emit(
                cppdiag::Severity::error,
                state.source,
                data.origin,
                "Unsolved type variable: ?{}",
                data.tag.get());
            state.set_solution(diagnostics, data, hir::type::Error {});
        }
    }
}

auto libresolve::resolve_class_reference(
    Context&                    context,
    Inference_state&            state,
    Scope&                      scope,
    Environment_wrapper const   environment,
    ast::Class_reference const& class_reference) -> hir::Class_reference
{
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    (void)class_reference;
    cpputil::todo();
}
