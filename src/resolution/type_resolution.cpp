#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    using namespace resolution;


    struct Type_resolution_visitor {
        Context  & context;
        Scope    & scope;
        Namespace& space;
        hir::Type& this_type;

        auto recurse(hir::Type& type) -> mir::Type {
            return context.resolve_type(type, scope, space);
        }
        auto recurse() noexcept {
            return [this](hir::Type& type) -> mir::Type {
                return recurse(type);
            };
        }


        auto operator()(hir::type::Integer const integer) -> mir::Type {
            static constexpr auto integers = std::to_array({
                &Context::i8_type, &Context::i16_type, &Context::i32_type, &Context::i64_type,
                &Context::u8_type, &Context::u16_type, &Context::u32_type, &Context::u64_type,
            });
            static_assert(integers.size() == static_cast<utl::Usize>(hir::type::Integer::_integer_count));
            return (context.*integers[static_cast<utl::Usize>(integer)])(this_type.source_view);
        }
        auto operator()(hir::type::String&) -> mir::Type {
            return context.string_type(this_type.source_view);
        }
        auto operator()(hir::type::Floating&) -> mir::Type {
            return context.floating_type(this_type.source_view);
        }
        auto operator()(hir::type::Character&) -> mir::Type {
            return context.character_type(this_type.source_view);
        }
        auto operator()(hir::type::Boolean&) -> mir::Type {
            return context.boolean_type(this_type.source_view);
        }

        auto operator()(hir::type::Self&) -> mir::Type {
            if (context.current_self_type.has_value()) {
                return *context.current_self_type;
            }
            else {
                context.error(this_type.source_view, {
                    "The Self type is only accessible within classes, 'impl' blocks, or 'inst' blocks"
                });
            }
        }

        auto operator()(hir::type::Tuple& tuple) -> mir::Type {
            return {
                .value       = wrap_type(mir::type::Tuple { .field_types = utl::map(recurse(), tuple.field_types) }),
                .source_view = this_type.source_view
            };
        }

        auto operator()(hir::type::Array& array) -> mir::Type {
            mir::Type const element_type = recurse(*array.element_type);
            mir::Expression length = context.resolve_expression(*array.array_length, scope, space);

            context.solve(constraint::Type_equality {
                .constrainer_type = context.size_type(this_type.source_view),
                .constrained_type = length.type,
                .constrained_note {
                    length.source_view,
                    "The array length must be of type {0}, but found {1}"
                }
            });

            return {
                .value = wrap_type(mir::type::Array {
                    .element_type = element_type,
                    .array_length = utl::wrap(std::move(length))
                }),
                .source_view = this_type.source_view
            };
        }

        auto operator()(hir::type::Typeof& typeof_) -> mir::Type {
            auto child_scope = scope.make_child();
            return context.resolve_expression(*typeof_.inspected_expression, child_scope, space).type.with(this_type.source_view);
        }

        auto operator()(hir::type::Typename& type) -> mir::Type {
            if (type.name.is_unqualified()) {
                if (auto* const binding = scope.find_type(type.name.primary_name.identifier)) {
                    binding->has_been_mentioned = true;
                    return binding->type.with(this_type.source_view);
                }
            }

            return utl::match(
                context.find_upper(type.name, scope, space),

                [this](utl::Wrapper<Struct_info> const info) -> mir::Type {
                    return info->structure_type.with(this_type.source_view);
                },
                [this](utl::Wrapper<Enum_info> const info) -> mir::Type {
                    return info->enumeration_type.with(this_type.source_view);
                },
                [this](utl::Wrapper<Alias_info> const info) -> mir::Type {
                    return context.resolve_alias(info).aliased_type.with(this_type.source_view);
                },

                [this](utl::Wrapper<Struct_template_info> const info) -> mir::Type {
                    return {
                        .value = wrap_type(mir::type::Structure {
                            .info           = context.instantiate_struct_template_with_synthetic_arguments(info, this_type.source_view),
                            .is_application = true
                        }),
                        .source_view = this_type.source_view
                    };
                },
                [this](utl::Wrapper<Enum_template_info> const info) -> mir::Type {
                    return {
                        .value = wrap_type(mir::type::Enumeration {
                            .info           = context.instantiate_enum_template_with_synthetic_arguments(info, this_type.source_view),
                            .is_application = true
                        }),
                        .source_view = this_type.source_view
                    };
                },
                [&](utl::Wrapper<Alias_template_info> const info) -> mir::Type {
                    return context.resolve_alias(
                        context.instantiate_alias_template_with_synthetic_arguments(info, this_type.source_view)
                    ).aliased_type.with(this_type.source_view);
                },

                [](utl::Wrapper<Typeclass_info>) -> mir::Type {
                    utl::todo();
                },
                [](utl::Wrapper<Typeclass_template_info>) -> mir::Type {
                    utl::todo();
                }
            );
        }

        auto operator()(hir::type::Reference& reference) -> mir::Type {
            return {
                .value = wrap_type(mir::type::Reference {
                    .mutability      = context.resolve_mutability(reference.mutability, scope),
                    .referenced_type = recurse(*reference.referenced_type)
                }),
                .source_view = this_type.source_view
            };
        }

        auto operator()(hir::type::Pointer& pointer) -> mir::Type {
            return {
                .value = wrap_type(mir::type::Pointer {
                    .mutability      = context.resolve_mutability(pointer.mutability, scope),
                    .pointed_to_type = recurse(*pointer.pointed_to_type)
                }),
                .source_view = this_type.source_view
            };
        }

        auto operator()(hir::type::Function& function) -> mir::Type {
            return {
                .value = wrap_type(mir::type::Function {
                    .parameter_types = utl::map(recurse(), function.argument_types),
                    .return_type     = recurse(*function.return_type)
                }),
                .source_view = this_type.source_view
            };
        }

        auto operator()(hir::type::Template_application& application) -> mir::Type {
            return utl::match(
                context.find_upper(application.name, scope, space),

                [&](utl::Wrapper<Struct_template_info> const info) -> mir::Type {
                    return {
                        .value = wrap_type(mir::type::Structure {
                            .info = context.instantiate_struct_template(
                                info, application.arguments, this_type.source_view, scope, space),
                            .is_application = true
                        }),
                        .source_view = this_type.source_view
                    };
                },
                [&](utl::Wrapper<Enum_template_info> info) -> mir::Type {
                    return {
                        .value = wrap_type(mir::type::Enumeration {
                            .info = context.instantiate_enum_template(
                                info, application.arguments, this_type.source_view, scope, space),
                            .is_application = true
                        }),
                        .source_view = this_type.source_view
                    };
                },

                [&](utl::Wrapper<Alias_template_info> info) -> mir::Type {
                    return context.resolve_alias(
                        context.instantiate_alias_template(info, application.arguments, this_type.source_view, scope, space)
                    ).aliased_type.with(this_type.source_view);
                },
                [this](utl::Wrapper<Typeclass_template_info>) -> mir::Type {
                    context.error(this_type.source_view, { "Expected a type, but found a typeclass" });
                },
                
                [this](utl::wrapper auto) -> mir::Type {
                    context.error(this_type.source_view, { "Template argument list applied to a non-template entity" });
                }
            );
        }

        auto operator()(hir::type::Wildcard) {
            return context.fresh_general_unification_type_variable(this_type.source_view);
        }

        auto operator()(auto&) -> mir::Type {
            context.error(this_type.source_view, { "This type can not be resolved yet" });
        }
    };

}


auto resolution::Context::resolve_type(hir::Type& type, Scope& scope, Namespace& space) -> mir::Type {
    return std::visit(Type_resolution_visitor { *this, scope, space, type }, type.value);
}