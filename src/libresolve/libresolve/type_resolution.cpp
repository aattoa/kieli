#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;


namespace {
    struct Type_resolution_visitor {
        Context  & context;
        Scope    & scope;
        Namespace& space;
        ast::Type& this_type;

        auto recurse(ast::Type& type) -> hir::Type {
            return context.resolve_type(type, scope, space);
        }
        auto recurse() noexcept {
            return [this](ast::Type& type) -> hir::Type {
                return recurse(type);
            };
        }


        auto operator()(compiler::built_in_type::Integer const integer) -> hir::Type {
            static constexpr auto integers = std::to_array({
                &Context::i8_type, &Context::i16_type, &Context::i32_type, &Context::i64_type,
                &Context::u8_type, &Context::u16_type, &Context::u32_type, &Context::u64_type,
            });
            static_assert(integers.size() == utl::enumerator_count<compiler::built_in_type::Integer>);
            return (context.*integers[utl::as_index(integer)])(this_type.source_view);
        }
        auto operator()(compiler::built_in_type::String) -> hir::Type {
            return context.string_type(this_type.source_view);
        }
        auto operator()(compiler::built_in_type::Floating) -> hir::Type {
            return context.floating_type(this_type.source_view);
        }
        auto operator()(compiler::built_in_type::Character) -> hir::Type {
            return context.character_type(this_type.source_view);
        }
        auto operator()(compiler::built_in_type::Boolean) -> hir::Type {
            return context.boolean_type(this_type.source_view);
        }

        auto operator()(ast::type::Self&) -> hir::Type {
            if (context.current_self_type.has_value())
                return *context.current_self_type;
            context.error(this_type.source_view, {
                "The Self type is only accessible within classes, 'impl' blocks, or 'inst' blocks"
            });
        }

        auto operator()(ast::type::Tuple& tuple) -> hir::Type {
            if (tuple.field_types.empty())
                return context.unit_type(this_type.source_view);
            return hir::Type {
                context.wrap_type(hir::type::Tuple { .field_types = utl::map(recurse(), tuple.field_types) }),
                this_type.source_view
            };
        }

        auto operator()(ast::type::Array& array) -> hir::Type {
            hir::Type const element_type = recurse(*array.element_type);
            hir::Expression length = context.resolve_expression(*array.array_length, scope, space);

            context.solve(constraint::Type_equality {
                .constrainer_type = context.size_type(this_type.source_view),
                .constrained_type = length.type,
                .constrained_note {
                    length.source_view,
                    "The array length must be of type {0}, but found {1}"
                }
            });

            return hir::Type {
                context.wrap_type(hir::type::Array {
                    .element_type = element_type,
                    .array_length = context.wrap(std::move(length))
                }),
                this_type.source_view
            };
        }

        auto operator()(ast::type::Typeof& typeof_) -> hir::Type {
            auto child_scope = scope.make_child();
            return context.resolve_expression(*typeof_.inspected_expression, child_scope, space).type.with(this_type.source_view);
        }

        auto operator()(ast::type::Typename& type) -> hir::Type {
            if (type.name.is_unqualified()) {
                if (auto* const binding = scope.find_type(type.name.primary_name.identifier)) {
                    binding->has_been_mentioned = true;
                    return binding->type.with(this_type.source_view);
                }
            }

            return utl::match(context.find_upper(type.name, scope, space),
                [this](utl::Wrapper<Struct_info> const info) -> hir::Type {
                    return info->structure_type.with(this_type.source_view);
                },
                [this](utl::Wrapper<Enum_info> const info) -> hir::Type {
                    return info->enumeration_type.with(this_type.source_view);
                },
                [this](utl::Wrapper<Alias_info> const info) -> hir::Type {
                    return context.resolve_alias(info).aliased_type.with(this_type.source_view);
                },

                [this](utl::Wrapper<Struct_template_info> const info) -> hir::Type {
                    return hir::Type {
                        context.wrap_type(hir::type::Structure {
                            .info           = context.instantiate_struct_template_with_synthetic_arguments(info, this_type.source_view),
                            .is_application = true,
                        }),
                        this_type.source_view,
                    };
                },
                [this](utl::Wrapper<Enum_template_info> const info) -> hir::Type {
                    return hir::Type {
                        context.wrap_type(hir::type::Enumeration {
                            .info           = context.instantiate_enum_template_with_synthetic_arguments(info, this_type.source_view),
                            .is_application = true,
                        }),
                        this_type.source_view,
                    };
                },
                [&](utl::Wrapper<Alias_template_info> const info) -> hir::Type {
                    return context.resolve_alias(
                        context.instantiate_alias_template_with_synthetic_arguments(info, this_type.source_view)
                    ).aliased_type.with(this_type.source_view);
                },

                [](utl::Wrapper<Typeclass_info>) -> hir::Type {
                    utl::todo();
                },
                [](utl::Wrapper<Typeclass_template_info>) -> hir::Type {
                    utl::todo();
                });
        }

        auto operator()(ast::type::Reference& reference) -> hir::Type {
            return hir::Type {
                context.wrap_type(hir::type::Reference {
                    .mutability      = context.resolve_mutability(reference.mutability, scope),
                    .referenced_type = recurse(*reference.referenced_type)
                }),
                this_type.source_view,
            };
        }

        auto operator()(ast::type::Pointer& pointer) -> hir::Type {
            return hir::Type {
                context.wrap_type(hir::type::Pointer {
                    .mutability      = context.resolve_mutability(pointer.mutability, scope),
                    .pointed_to_type = recurse(*pointer.pointed_to_type)
                }),
                this_type.source_view,
            };
        }

        auto operator()(ast::type::Function& function) -> hir::Type {
            return hir::Type {
                context.wrap_type(hir::type::Function {
                    .parameter_types = utl::map(recurse(), function.argument_types),
                    .return_type     = recurse(*function.return_type)
                }),
                this_type.source_view,
            };
        }

        auto operator()(ast::type::Template_application& application) -> hir::Type {
            return utl::match(context.find_upper(application.name, scope, space),
                [&](utl::Wrapper<Struct_template_info> const info) -> hir::Type {
                    return hir::Type {
                        context.wrap_type(hir::type::Structure {
                            .info = context.instantiate_struct_template(
                                info, application.arguments, this_type.source_view, scope, space),
                            .is_application = true
                        }),
                        this_type.source_view,
                    };
                },
                [&](utl::Wrapper<Enum_template_info> info) -> hir::Type {
                    return hir::Type {
                        context.wrap_type(hir::type::Enumeration {
                            .info = context.instantiate_enum_template(
                                info, application.arguments, this_type.source_view, scope, space),
                            .is_application = true,
                        }),
                        this_type.source_view,
                    };
                },

                [&](utl::Wrapper<Alias_template_info> info) -> hir::Type {
                    return context.resolve_alias(
                        context.instantiate_alias_template(info, application.arguments, this_type.source_view, scope, space)
                    ).aliased_type.with(this_type.source_view);
                },
                [this](utl::Wrapper<Typeclass_template_info>) -> hir::Type {
                    context.error(this_type.source_view, { "Expected a type, but found a typeclass" });
                },
                
                [this](utl::wrapper auto) -> hir::Type {
                    context.error(this_type.source_view, { "Template argument list applied to a non-template entity" });
                }
            );
        }

        auto operator()(ast::type::Wildcard) {
            return context.fresh_general_unification_type_variable(this_type.source_view);
        }

        auto operator()(auto&) -> hir::Type {
            context.error(this_type.source_view, { "This type can not be resolved yet" });
        }
    };
}


auto libresolve::Context::resolve_type(ast::Type& type, Scope& scope, Namespace& space) -> hir::Type {
    return std::visit(Type_resolution_visitor { *this, scope, space, type }, type.value);
}
