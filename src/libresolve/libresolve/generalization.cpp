#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    struct Generalization_type_visitor {
        using Unification_variable_handler
            = std::function<void(hir::Type variable, hir::Unification_type_variable_state&)>;

        Context&                      context;
        Unification_variable_handler& unification_variable_handler;
        hir::Type                     this_type;

        auto recurse() noexcept
        {
            return [*this](hir::Type const type) mutable -> void {
                this_type = type;
                std::visit(*this, *type.flattened_value());
            };
        }

        auto recurse(hir::Type const type) -> void
        {
            return recurse()(type);
        }

        auto operator()(hir::type::Unification_variable const variable) -> void
        {
            unification_variable_handler(this_type, *variable.state);
        }

        auto operator()(hir::type::Tuple const& tuple) -> void
        {
            ranges::for_each(tuple.field_types, recurse());
        }

        auto operator()(hir::type::Array const& array) -> void
        {
            recurse(array.element_type);
            recurse(array.array_length->type);
        }

        auto operator()(
            utl::one_of<hir::type::Structure, hir::type::Enumeration> auto const& user_defined)
            -> void
        {
            if (user_defined.is_application) {
                for (hir::Template_argument const& argument :
                     utl::get(user_defined.info->template_instantiation_info).template_arguments)
                {
                    utl::match(
                        argument.value,
                        [&](hir::Type const type) { recurse(type); },
                        [&](hir::Expression const& expression) { recurse(expression.type); },
                        [&](hir::Mutability const&) {});
                }
            }
        }

        auto operator()(hir::type::Function const& function) -> void
        {
            recurse(function.return_type);
            ranges::for_each(function.parameter_types, recurse());
        }

        auto operator()(hir::type::Reference const& reference) -> void
        {
            recurse(reference.referenced_type);
        }

        auto operator()(hir::type::Pointer const& pointer) -> void
        {
            recurse(pointer.pointed_to_type);
        }

        auto operator()(hir::type::Slice const& slice) -> void
        {
            recurse(slice.element_type);
        }

        auto operator()(utl::one_of<
                        hir::type::Template_parameter_reference,
                        hir::type::Self_placeholder,
                        compiler::built_in_type::Integer,
                        compiler::built_in_type::Floating,
                        compiler::built_in_type::String,
                        compiler::built_in_type::Character,
                        compiler::built_in_type::Boolean> auto const&) -> void
        {
            // terminal
        }
    };
} // namespace

auto libresolve::Context::generalize_to(
    hir::Type const type, std::vector<hir::Template_parameter>& output) -> void
{
    std::function handler = [&](hir::Type const type, hir::Unification_type_variable_state& state) {
        auto&      unsolved = state.as_unsolved();
        auto const tag      = fresh_template_parameter_reference_tag();

        output.push_back(hir::Template_parameter {
            .value = hir::Template_parameter::Type_parameter {
                .classes = std::move(unsolved.classes),
                .name    = tl::nullopt, // Implicit template parameters have no name
            },
            .default_argument = hir::Template_default_argument {
                .argument = { ast::Template_argument::Wildcard { type.source_view() } },
                .scope    = nullptr, // Wildcard arguments need no scope
            },
            .reference_tag = tag,
            .source_view   = type.source_view(),
        });
        state.solve_with(hir::Type {
            wrap_type(hir::type::Template_parameter_reference {
                .identifier = { tl::nullopt },
                .tag        = tag,
            }),
            type.source_view(),
        });
    };
    std::visit(Generalization_type_visitor { *this, handler, type }, *type.flattened_value());
}

auto libresolve::Context::ensure_non_generalizable(
    hir::Type const type, std::string_view const type_description) -> void
{
    std::function handler = [&](hir::Type const type, hir::Unification_type_variable_state&) {
        error(
            type.source_view(),
            {
                .message = "{}'s type contains an unsolved unification type variable: {}"_format(
                    type_description, hir::to_string(type)),
                .help_note = "This can most likely be fixed by providing explicit type annotations",
            });
    };
    std::visit(Generalization_type_visitor { *this, handler, type }, *type.flattened_value());
}
