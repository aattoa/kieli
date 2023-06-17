#include <libutl/common/utilities.hpp>
#include <libreify/reification_internals.hpp>

using namespace libreify;


namespace {

    struct Type_reification_visitor {
        Context&  context;
        mir::Type this_type;

        [[nodiscard]]
        auto recurse() const noexcept {
            return std::bind_front(&Context::reify_type, &context);
        }


        auto operator()(mir::type::Unification_variable const& variable) -> cir::Type {
            context.error(this_type.source_view(), {
                .message = fmt::format(
                    "Found an unsolved type variable: {}",
                    mir::to_string(variable.state->as_unsolved().tag)),
            });
        }

        auto operator()(compiler::built_in_type::Integer const integer) -> cir::Type {
            static constexpr auto types = std::to_array({
                &Context::i8_type, &Context::i16_type, &Context::i32_type, &Context::i64_type,
                &Context::u8_type, &Context::u16_type, &Context::u32_type, &Context::u64_type,
            });
            static_assert(types.size() == utl::enumerator_count<compiler::built_in_type::Integer>);
            return (context.*types[utl::as_index(integer)])(this_type.source_view());
        }

        auto operator()(compiler::built_in_type::Boolean const&) -> cir::Type {
            return context.boolean_type(this_type.source_view());
        }
        auto operator()(compiler::built_in_type::Floating const&) -> cir::Type {
            return context.floating_type(this_type.source_view());
        }
        auto operator()(compiler::built_in_type::String const&) -> cir::Type {
            return context.string_type(this_type.source_view());
        }
        auto operator()(compiler::built_in_type::Character const&) -> cir::Type {
            return context.character_type(this_type.source_view());
        }

        auto operator()(utl::one_of<mir::type::Pointer, mir::type::Reference> auto const& pointer) -> cir::Type {
            // The structured binding avoids having to spell out the field
            // names, which are different for reference and pointer nodes.
            auto [mutability, pointed_to_type] = pointer;
            return {
                .value       = context.wrap_type(cir::type::Pointer { .pointed_to_type = context.reify_type(pointed_to_type) }),
                .size        = sizeof(void*),
                .source_view = this_type.source_view()
            };
        }

        auto operator()(mir::type::Tuple const& tuple) -> cir::Type {
            auto field_types
                = tuple.field_types
                | ranges::views::transform(recurse())
                | ranges::to<std::vector>();

            auto const size = ranges::fold_left(
                ranges::views::transform(field_types, &cir::Type::size),
                utl::Safe_usize {},
                std::plus {});

            return {
                .value       = context.wrap_type(cir::type::Tuple { .field_types = std::move(field_types) }),
                .size        = size,
                .source_view = this_type.source_view()
            };
        }

        auto operator()(mir::type::Array const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Enumeration const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Structure const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Function const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Self_placeholder const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Slice const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Template_parameter_reference const&) -> cir::Type {
            utl::todo();
        }
    };

}


auto libreify::Context::reify_type(mir::Type type) -> cir::Type {
    return std::visit(Type_reification_visitor { *this, type }, *type.flattened_value());
}
