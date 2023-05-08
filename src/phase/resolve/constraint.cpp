#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    auto report_type_unification_failure(
        resolution::Context&                        context,
        resolution::constraint::Type_equality const constraint,
        mir::Type                             const left,
        mir::Type                             const right) -> void
    {
        auto sections = utl::vector_with_capacity<utl::diagnostics::Text_section>(2);

        tl::optional<std::string> const constrainer_note = constraint.constrainer_note.transform(
            [&](resolution::constraint::Explanation const explanation) {
                return fmt::vformat(explanation.explanatory_note, fmt::make_format_args(constraint.constrainer_type, constraint.constrained_type));
            });

        std::string const constrained_note =
            fmt::vformat(constraint.constrained_note.explanatory_note, fmt::make_format_args(constraint.constrainer_type, constraint.constrained_type));

        if (constrainer_note.has_value()) {
            sections.push_back(utl::diagnostics::Text_section{
                .source_view = constraint.constrainer_note->source_view,
                .note        = *constrainer_note,
                .note_color  = utl::diagnostics::warning_color
            });
        }

        sections.push_back(utl::diagnostics::Text_section{
            .source_view = constraint.constrained_note.source_view,
            .note        = constrained_note,
            .note_color  = utl::diagnostics::error_color
        });

        context.diagnostics().emit_error({
            .sections          = std::move(sections),
            .message           = "Could not unify {} ~ {}",
            .message_arguments = fmt::make_format_args(left, right)
        });
    }

    auto report_recursive_type(
        resolution::Context&                        context,
        resolution::constraint::Type_equality const constraint,
        mir::Type                             const variable,
        mir::Type                             const solution) -> void
    {
        context.error(constraint.constrained_type.source_view(), {
            .message           = "Recursive type variable solution: {} = {}",
            .message_arguments = fmt::make_format_args(variable, solution)
        });
    }

    auto report_mutability_unification_failure(
        resolution::Context&                              context,
        resolution::constraint::Mutability_equality const constraint) -> void
    {
        mir::Mutability const left  = constraint.constrainer_mutability;
        mir::Mutability const right = constraint.constrained_mutability;

        std::string const constrainer_note =
            fmt::vformat(constraint.constrainer_note.explanatory_note, fmt::make_format_args(left, right));
        std::string const constrained_note =
            fmt::vformat(constraint.constrained_note.explanatory_note, fmt::make_format_args(left, right));

        context.diagnostics().emit_error({
            .sections = utl::to_vector<utl::diagnostics::Text_section>({
                {
                    .source_view = constraint.constrainer_note.source_view,
                    .note        = constrainer_note,
                    .note_color  = utl::diagnostics::warning_color
                },
                {
                    .source_view = constraint.constrained_note.source_view,
                    .note        = constrained_note,
                    .note_color  = utl::diagnostics::error_color
                }
            }),
            .message           = "Could not unify {} ~ {}",
            .message_arguments = fmt::make_format_args(left, right)
        });
    }

}


auto resolution::Context::solve(constraint::Type_equality const& constraint) -> void {
    utl::always_assert(unify_types({
        .constraint_to_be_tested    = constraint,
        .allow_coercion             = true,
        .do_destructive_unification = true,
        .report_unification_failure = report_type_unification_failure,
        .report_recursive_type      = report_recursive_type,
    }));
}


auto resolution::Context::solve(constraint::Mutability_equality const& constraint) -> void {
    utl::always_assert(unify_mutabilities({
        .constraint_to_be_tested    = constraint,
        .allow_coercion             = true,
        .do_destructive_unification = true,
        .report_unification_failure = report_mutability_unification_failure,
    }));
}


auto resolution::Context::solve(constraint::Instance const&) -> void {
    utl::todo();
}


auto resolution::Context::solve(constraint::Struct_field const& constraint) -> void {
    if (auto const* const type = std::get_if<mir::type::Structure>(&*constraint.struct_type.flattened_value())) {
        mir::Struct const& structure = resolve_struct(type->info);
        for (mir::Struct::Member const& member : structure.members) {
            if (constraint.field_identifier == member.name.identifier) {
                solve(constraint::Type_equality {
                    .constrainer_type = member.type,
                    .constrained_type = constraint.field_type,
                    .constrained_note {
                        constraint.explanation.source_view,
                        "(this message should never be visible)"
                    }
                });
                return;
            }
        }
        error(constraint.explanation.source_view, {
            .message             = constraint.explanation.explanatory_note,
            .help_note           = "{} does not have a member '{}'",
            .help_note_arguments = fmt::make_format_args(constraint.struct_type, constraint.field_identifier)
        });
    }
    else {
        error(constraint.explanation.source_view, {
            .message             = constraint.explanation.explanatory_note,
            .help_note           = "{} is not a struct type, so it does not have named fields",
            .help_note_arguments = fmt::make_format_args(constraint.struct_type)
        });
    }
}


auto resolution::Context::solve(constraint::Tuple_field const& constraint) -> void {
    if (auto const* const type = std::get_if<mir::type::Tuple>(&*constraint.tuple_type.flattened_value())) {
        if (constraint.field_index.get() >= type->field_types.size()) {
            error(constraint.explanation.source_view, {
                .message             = constraint.explanation.explanatory_note,
                .help_note           = "{} does not have a {} field",
                .help_note_arguments = fmt::make_format_args(constraint.tuple_type, utl::formatting::integer_with_ordinal_indicator(constraint.field_index.get() + 1))
            });
        }
        solve(constraint::Type_equality {
            .constrainer_type = constraint.field_type,
            .constrained_type = type->field_types[constraint.field_index.get()],
            .constrained_note {
                constraint.explanation.source_view,
                "(this message should never be visible)"
            }
        });
    }
    else {
        error(constraint.explanation.source_view, {
            .message             = constraint.explanation.explanatory_note,
            .help_note           = "{} is not a tuple type, so it does not have indexed fields",
            .help_note_arguments = fmt::make_format_args(constraint.tuple_type)
        });
    }
}
