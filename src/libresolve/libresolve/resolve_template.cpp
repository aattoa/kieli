#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    struct Template_parameter_resolution_visitor {
        Context&               context;
        Inference_state&       state;
        Scope&                 scope;
        Environment_wrapper    environment;
        Template_parameter_tag tag;

        template <class Default>
        auto resolve_default_argument()
        {
            return [&](auto const& argument) {
                return std::visit<Default>(
                    utl::Overload {
                        [&](ast::Wildcard const& wildcard) {
                            return hir::Wildcard { .source_range = wildcard.source_range };
                        },

                        [&](utl::Wrapper<ast::Type> const type) {
                            return resolve_type(context, state, scope, environment, *type);
                        },

                        [&](ast::Mutability const& mutability) {
                            return resolve_mutability(context, scope, mutability);
                        },
                    },
                    argument);
            };
        }

        auto operator()(ast::Template_type_parameter const& parameter)
            -> hir::Template_parameter::Variant
        {
            scope.bind_type(
                parameter.name.identifier,
                Type_bind {
                    .name = parameter.name,
                    .type {
                        context.arenas.type(hir::type::Parameterized { .tag = tag }),
                        parameter.name.source_range,
                    },
                });
            auto const resolve_class = [&](ast::Class_reference const& class_reference) {
                return resolve_class_reference(context, state, scope, environment, class_reference);
            };
            return hir::Template_type_parameter {
                .classes = std::views::transform(parameter.classes, resolve_class)
                         | std::ranges::to<std::vector>(),
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_type_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_mutability_parameter const& parameter)
            -> hir::Template_parameter::Variant
        {
            scope.bind_mutability(
                parameter.name.identifier,
                Mutability_bind {
                    .name = parameter.name,
                    .mutability {
                        context.arenas.mutability(hir::mutability::Parameterized { .tag = tag }),
                        parameter.name.source_range,
                    },
                });
            return hir::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(
                    resolve_default_argument<hir::Template_mutability_parameter::Default>()),
            };
        }

        auto operator()(ast::Template_value_parameter const& parameter)
            -> hir::Template_parameter::Variant
        {
            context.compile_info.diagnostics.error(
                scope.source(),
                parameter.name.source_range,
                "Template value parameters are not supported yet");
        }
    };
} // namespace

auto libresolve::resolve_template_parameters(
    Context&                        context,
    Inference_state&                state,
    Scope&                          scope,
    Environment_wrapper             environment,
    ast::Template_parameters const& parameters) -> std::vector<hir::Template_parameter>
{
    auto const resolve_parameter = [&](ast::Template_parameter const& parameter) {
        auto const tag = context.tag_state.fresh_template_parameter_tag();
        return hir::Template_parameter {
            .variant = std::visit(
                Template_parameter_resolution_visitor { context, state, scope, environment, tag },
                parameter.variant),
            .tag          = tag,
            .source_range = parameter.source_range,
        };
    };
    return parameters
        .transform(std::views::transform(resolve_parameter) | std::ranges::to<std::vector>())
        .value_or(std::vector<hir::Template_parameter> {});
}

auto libresolve::resolve_template_arguments(
    Context&                                    context,
    Inference_state&                            state,
    Scope&                                      scope,
    Environment_wrapper                         environment,
    std::vector<hir::Template_parameter> const& parameters,
    std::vector<ast::Template_argument> const&  arguments) -> std::vector<hir::Template_argument>
{
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    (void)parameters;
    (void)arguments;
    cpputil::todo();
}