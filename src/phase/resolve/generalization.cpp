#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {
    struct Generalization_type_visitor {
        using Unification_variable_handler =
            std::function<void(mir::Type variable, mir::Unification_type_variable_state&)>;

        resolution::Context&          context;
        mir::Type                     this_type;
        Unification_variable_handler& unification_variable_handler;

        auto recurse() noexcept {
            return [*this](mir::Type const type) mutable -> void {
                this_type = type;
                std::visit(*this, *type.flattened_value());
            };
        }
        auto recurse(mir::Type const type) -> void {
            return recurse()(type);
        }

        auto operator()(mir::type::Unification_variable const variable) -> void {
            unification_variable_handler(this_type, *variable.state);
        }
        auto operator()(mir::type::Tuple const& tuple) -> void {
            ranges::for_each(tuple.field_types, recurse());
        }
        auto operator()(mir::type::Array const& array) -> void {
            recurse(array.element_type);
            recurse(array.array_length->type);
        }
        auto operator()(utl::one_of<mir::type::Structure, mir::type::Enumeration> auto const& user_defined) -> void {
            if (user_defined.is_application) {
                for (mir::Template_argument const& argument : utl::get(user_defined.info->template_instantiation_info).template_arguments) {
                    utl::match(argument.value,
                        [&](mir::Type const type)              { recurse(type); },
                        [&](mir::Expression const& expression) { recurse(expression.type); },
                        [&](mir::Mutability const&)            {});
                }
            }
        }
        auto operator()(mir::type::Function const& function) -> void {
            recurse(function.return_type);
            ranges::for_each(function.parameter_types, recurse());
        }
        auto operator()(mir::type::Reference const& reference) -> void {
            recurse(reference.referenced_type);
        }
        auto operator()(mir::type::Pointer const& pointer) -> void {
            recurse(pointer.pointed_to_type);
        }
        auto operator()(mir::type::Slice const& slice) -> void {
            recurse(slice.element_type);
        }
        auto operator()(utl::one_of<
            mir::type::Template_parameter_reference,
            mir::type::Self_placeholder,
            mir::type::Integer,
            mir::type::Floating,
            mir::type::String,
            mir::type::Character,
            mir::type::Boolean> auto const&) -> void {}
    };
}


auto resolution::Context::generalize_to(mir::Type const type, std::vector<mir::Template_parameter>& output) -> void {
    std::function handler = [&](mir::Type const type, mir::Unification_type_variable_state& state) {
        auto& unsolved = state.as_unsolved();
        auto const tag = fresh_template_parameter_reference_tag();

        output.push_back(mir::Template_parameter {
            .value            = mir::Template_parameter::Type_parameter { .classes = std::move(unsolved.classes) },
            .name             = { tl::nullopt },
            .default_argument = mir::Template_default_argument { .argument = { hir::Template_argument::Wildcard { type.source_view() } } },
            .reference_tag    = tag,
            .source_view      = type.source_view(),
        });
        state.solve_with(mir::Type {
            wrap_type(mir::type::Template_parameter_reference {
                .identifier = { tl::nullopt },
                .tag        = tag,
            }),
            type.source_view(),
        });
    };
    std::visit(Generalization_type_visitor { *this, type, handler }, *type.flattened_value());
}


auto resolution::Context::ensure_non_generalizable(mir::Type const type, std::string_view const type_description) -> void {
    std::function handler = [&](mir::Type const type, mir::Unification_type_variable_state&) {
        error(type.source_view(), {
            .message           = "{}'s type contains an unsolved unification type variable: {}",
            .message_arguments = fmt::make_format_args(type_description, type),
            .help_note         = "This can most likely be fixed by providing explicit type annotations",
        });
    };
    std::visit(Generalization_type_visitor { *this, type, handler }, *type.flattened_value());
}
